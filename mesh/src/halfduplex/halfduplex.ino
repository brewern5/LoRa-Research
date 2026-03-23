#include <Arduino.h>

#include "../src/models/packet.h"
#include "../src/storage/SdManager.h"
#include "../src/comms/LoraManager.h"
#include "../src/app/ResearchStateMachine.h"
#include "../src/display/StatusDisplay.h"

SdManager sdMgr;
bool g_sd_ready = false;

LoRaManager lora;
ResearchStateMachine state("HD");

uint16_t g_session_id;
uint16_t g_seq_num;
uint32_t timeout_ms = static_cast<uint32_t>(MESH_ACK_TIMEOUT_MS);

// Placeholder location until GPS integration is added
constexpr float kDefaultLat = 0.0f;
constexpr float kDefaultLon = 0.0f;

constexpr const char* kPayloadFile = "lora_payload_new.bin";
constexpr uint8_t kPayloadCodec = CODEC_RAW_PCM;
constexpr uint16_t kPayloadSampleHz = 8000;
constexpr uint16_t kPayloadDurationMs = 0;
constexpr uint8_t kMaxAckRetries = 2;
constexpr uint32_t kButtonDebounceMs = 35;
constexpr uint32_t kIdleDisplayTickMs = 500;
constexpr uint32_t kIdleSerialTickMs = 2000;

#if MESH_TX_BUTTON_ACTIVE_LOW
constexpr uint8_t kButtonPressedLevel = LOW;
#else
constexpr uint8_t kButtonPressedLevel = HIGH;
#endif

static uint8_t g_lastButtonSample = HIGH;
static uint8_t g_stableButtonState = HIGH;
static uint32_t g_lastButtonChangeMs = 0;
static uint32_t g_lastIdleDisplayMs = 0;
static uint32_t g_lastIdleSerialMs = 0;
static uint8_t g_idleSpinner = 0;

static void withRetrySuffix(char* out, size_t outLen, const char* baseStatus, uint8_t retryIndex)
{
    snprintf(out, outLen, "%s_R%u", baseStatus, static_cast<unsigned>(retryIndex));
}

static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return crc;
}

static const char* packetTypeLabel(uint8_t packetType)
{
    switch (packetType)
    {
    case PKT_AUDIO_START: return "START";
    case PKT_AUDIO_DATA: return "DATA";
    case PKT_AUDIO_END: return "END";
    case PKT_ACK: return "ACK";
    default: return "UNKNOWN";
    }
}

static bool getPayloadStats(SdManager& manager, const char* filename,
                            uint32_t& totalSize, uint16_t& totalFrags, uint32_t& payloadCrc)
{
    totalSize = 0;
    totalFrags = 0;
    payloadCrc = 0;

    if (!manager.openAudioFile(filename))
    {
        Serial.printf("[HD] Unable to open payload file: %s\n", filename);
        return false;
    }

    uint32_t crcState = 0xFFFFFFFFUL;
    uint32_t fragCount = 0;
    AudioPacket packet;
    while (manager.readAudioChunk(packet))
    {
        if (packet.bytesRead <= 0)
        {
            continue;
        }
        totalSize += static_cast<uint32_t>(packet.bytesRead);
        crcState = crc32Update(crcState, packet.buffer, static_cast<size_t>(packet.bytesRead));
        fragCount++;
    }

    manager.closeAudioFile();

    if (totalSize == 0 || fragCount == 0)
    {
        Serial.println("[HD] Payload file is empty");
        return false;
    }

    if (fragCount > 0xFFFF)
    {
        Serial.printf("[HD] Payload too large, fragments=%lu exceeds protocol max\n", static_cast<unsigned long>(fragCount));
        return false;
    }

    totalFrags = static_cast<uint16_t>(fragCount);
    payloadCrc = ~crcState;
    return true;
}

static bool wasTxButtonPressed()
{
    const uint8_t sample = static_cast<uint8_t>(digitalRead(MESH_TX_BUTTON_PIN));
    const uint32_t now = millis();

    if (sample != g_lastButtonSample)
    {
        g_lastButtonSample = sample;
        g_lastButtonChangeMs = now;
    }

    if (g_stableButtonState != g_lastButtonSample && (now - g_lastButtonChangeMs) >= kButtonDebounceMs)
    {
        const uint8_t previousStable = g_stableButtonState;
        g_stableButtonState = g_lastButtonSample;
        return previousStable != kButtonPressedLevel && g_stableButtonState == kButtonPressedLevel;
    }

    return false;
}

static void publishIdleHeartbeat()
{
    const uint32_t now = millis();

    if ((now - g_lastIdleDisplayMs) >= kIdleDisplayTickMs)
    {
        static const char* kIdleFrames[] = {
            "Idle",
            "Idle .",
            "Idle ..",
            "Idle ..."
        };

        StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
        StatusDisplay::setMessage(kIdleFrames[g_idleSpinner]);
        g_idleSpinner = static_cast<uint8_t>((g_idleSpinner + 1U) % 4U);
        g_lastIdleDisplayMs = now;
    }

    if ((now - g_lastIdleSerialMs) >= kIdleSerialTickMs)
    {
        Serial.printf("[HD][IDLE] alive uptime=%lu ms\n", static_cast<unsigned long>(now));
        g_lastIdleSerialMs = now;
    }
}

static void processIncomingPacket()
{
    uint8_t raw[LORA_MAX_PAYLOAD] = {0};
    size_t receivedLen = 0;

    if (!lora.receiveRaw(raw, sizeof(raw), &receivedLen))
    {
        return;
    }

    state.transition(ResearchEvent::RECEIVE_WINDOW);
    StatusDisplay::setLoRa(StatusDisplay::LORA_RECEIVING);

    if (receivedLen < LORA_HEADER_SIZE)
    {
        Serial.printf("[HD][RX] Dropped short packet len=%u\n", static_cast<unsigned>(receivedLen));
        state.transition(ResearchEvent::RX_PACKET_DONE);
        StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
        return;
    }

    LoRaHeader hdr;
    deserializeHeader(raw, &hdr);
    const uint8_t packetType = getType(hdr.ver_type);
    const char* packetTypeStr = packetTypeLabel(packetType);

    const uint32_t rxTimeMs = millis();
    const int rssi = static_cast<int>(lora.getLastRSSI());
    const float snr = lora.getLastSNR();

    int16_t fragIndex = -1;
    uint16_t fragLen = 0;
    if (packetType == PKT_AUDIO_DATA)
    {
        fragIndex = static_cast<int16_t>(hdr.seq_num);
        fragLen = static_cast<uint16_t>(receivedLen - LORA_HEADER_SIZE);
    }

    Serial.printf("[HD][RX] type=%s seq=%u sess=0x%04X len=%u RSSI=%d SNR=%.1f\n",
                  packetTypeStr,
                  hdr.seq_num,
                  hdr.session_id,
                  static_cast<unsigned>(receivedLen),
                  rssi,
                  snr);

    StatusDisplay::onPacketReceived();

    if (g_sd_ready)
    {
        sdMgr.logTransmission(kDefaultLat, kDefaultLon, rxTimeMs, rxTimeMs, rssi, snr,
                              hdr.session_id, hdr.seq_num, fragIndex, fragLen,
                              packetTypeStr, "RX_RECV");
    }

    // ACK frames should never be ACKed back.
    if (packetType != PKT_ACK)
    {
        const uint32_t ackTimeMs = millis();
        const bool ackSent = lora.sendAckFor(hdr, ACK_STATUS_OK);

        if (g_sd_ready)
        {
            sdMgr.logTransmission(kDefaultLat, kDefaultLon, rxTimeMs, ackTimeMs, rssi, snr,
                                  hdr.session_id, hdr.seq_num, fragIndex, fragLen,
                                  packetTypeStr, ackSent ? "ACK_SENT" : "ACK_SEND_FAIL");
        }
    }

    state.transition(ResearchEvent::RX_PACKET_DONE);
    StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
}

static bool transmitPayloadFile()
{
    if (!g_sd_ready)
    {
        Serial.println("[HD][TX] SD not ready; cannot transmit payload file");
        StatusDisplay::setMessage("SD not ready");
        return false;
    }

    auto logAck = [&](uint32_t txTimeMs, bool ackOk, const char* packetType, const char* status,
                      int16_t fragIndex, uint16_t fragLen, uint8_t retryIndex)
    {
        if (!g_sd_ready)
        {
            return;
        }

        const uint32_t ackTimeMs = millis();
        const int rssi = ackOk ? static_cast<int>(lora.getLastRSSI()) : 0;
        const float snr = ackOk ? lora.getLastSNR() : 0.0f;
        const uint16_t seqNum = lora.getLastSeqNum();

        Serial.printf("[HD][LOG] type=%s status=%s retry=%u tx=%lu ack=%lu rssi=%d snr=%.1f\n",
                      packetType, status, static_cast<unsigned>(retryIndex), txTimeMs, ackTimeMs, rssi, snr);

        sdMgr.logTransmission(kDefaultLat, kDefaultLon, txTimeMs, ackTimeMs, rssi, snr,
                              g_session_id, seqNum, fragIndex, fragLen, packetType, status);
    };

    uint32_t payloadSize = 0;
    uint16_t total_frags = 0;
    uint32_t audio_crc = 0;
    if (!getPayloadStats(sdMgr, kPayloadFile, payloadSize, total_frags, audio_crc))
    {
        StatusDisplay::setMessage("Missing/invalid payload");
        return false;
    }

    if (!sdMgr.openAudioFile(kPayloadFile))
    {
        Serial.printf("[HD][TX] Failed to reopen payload file for transmit: %s\n", kPayloadFile);
        StatusDisplay::setMessage("Payload open failed");
        return false;
    }

    Serial.printf("[HD][TX] Starting transfer from %s: %lu bytes, %u fragments, CRC32=0x%08lX\n",
                  kPayloadFile,
                  static_cast<unsigned long>(payloadSize),
                  total_frags,
                  static_cast<unsigned long>(audio_crc));

    StatusDisplay::setLoRa(StatusDisplay::LORA_TRANSMITTING);
    StatusDisplay::setMessage("Button TX running...");

    bool startAckOk = false;
    for (uint8_t retry = 0; retry <= kMaxAckRetries; ++retry)
    {
        state.transition(ResearchEvent::PAYLOAD_AVAILABLE);
        const uint32_t startTxTime = millis();
        const bool startSent = lora.sendAudioStart(total_frags, kPayloadCodec, kPayloadSampleHz, kPayloadDurationMs, payloadSize);

        if (!startSent)
        {
            state.transition(ResearchEvent::TX_FAILED);
            char status[24];
            withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
            logAck(startTxTime, false, "START", status, -1, 0, retry);

            if (retry == kMaxAckRetries)
            {
                Serial.println("[HD][TX] START failed after retries");
                sdMgr.closeAudioFile();
                StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
                return false;
            }
            delay(50);
            continue;
        }

        state.transition(ResearchEvent::TX_COMPLETE);
        startAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);

        char status[24];
        withRetrySuffix(status, sizeof(status), startAckOk ? "ACK_OK" : "ACK_TIMEOUT", retry);
        logAck(startTxTime, startAckOk, "START", status, -1, 0, retry);

        if (startAckOk)
        {
            state.transition(ResearchEvent::ACK_VALID);
            break;
        }

        if (retry < kMaxAckRetries)
        {
            state.transition(ResearchEvent::ACK_TIMEOUT);
        }
        else
        {
            state.transition(ResearchEvent::RETRY_EXHAUSTED);
        }
        delay(50);
    }

    if (!startAckOk)
    {
        sdMgr.closeAudioFile();
        StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
        return false;
    }

    uint16_t dataOkCount = 0;
    uint16_t dataFailCount = 0;
    uint16_t frag = 0;
    AudioPacket packet;
    while (sdMgr.readAudioChunk(packet))
    {
        if (packet.bytesRead <= 0)
        {
            continue;
        }

        const uint16_t chunk = static_cast<uint16_t>(packet.bytesRead);
        bool dataAckOk = false;

        for (uint8_t retry = 0; retry <= kMaxAckRetries; ++retry)
        {
            state.transition(ResearchEvent::PAYLOAD_AVAILABLE);
            const uint32_t dataTxTime = millis();
            const bool dataSent = lora.sendAudioData(packet.buffer, static_cast<uint8_t>(chunk));

            if (!dataSent)
            {
                state.transition(ResearchEvent::TX_FAILED);
                char status[24];
                withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
                logAck(dataTxTime, false, "DATA", status,
                       static_cast<int16_t>(frag), static_cast<uint16_t>(chunk), retry);

                if (retry == kMaxAckRetries)
                {
                    dataFailCount++;
                    Serial.printf("[HD][TX] DATA frag %u failed after retries\n", frag);
                    StatusDisplay::setMessage("Failed data fragment");
                }
                else
                {
                    delay(50);
                }
                continue;
            }

            state.transition(ResearchEvent::TX_COMPLETE);
            dataAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);

            char status[24];
            withRetrySuffix(status, sizeof(status), dataAckOk ? "ACK_OK" : "ACK_TIMEOUT", retry);
            logAck(dataTxTime, dataAckOk, "DATA", status,
                   static_cast<int16_t>(frag), static_cast<uint16_t>(chunk), retry);

            if (dataAckOk)
            {
                state.transition(ResearchEvent::ACK_VALID);
                dataOkCount++;
                StatusDisplay::onPacketSent();
                break;
            }

            if (retry < kMaxAckRetries)
            {
                state.transition(ResearchEvent::ACK_TIMEOUT);
                delay(50);
            }
            else
            {
                state.transition(ResearchEvent::RETRY_EXHAUSTED);
                dataFailCount++;
                Serial.printf("[HD][TX] DATA frag %u no ACK after retries\n", frag);
                StatusDisplay::setMessage("Data ACK timeout");
            }
        }

        frag++;
        delay(50);
    }

    sdMgr.closeAudioFile();

    Serial.printf("[HD][TX] DATA summary: acked=%u failed_or_timeout=%u total=%u\n",
                  dataOkCount, dataFailCount, frag);

    bool endAckOk = false;
    for (uint8_t retry = 0; retry <= kMaxAckRetries; ++retry)
    {
        state.transition(ResearchEvent::PAYLOAD_AVAILABLE);
        const uint32_t endTxTime = millis();
        const bool endSentOk = lora.sendAudioEnd(frag, audio_crc);

        if (!endSentOk)
        {
            state.transition(ResearchEvent::TX_FAILED);
            char status[24];
            withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
            logAck(endTxTime, false, "END", status, -1, 0, retry);

            if (retry == kMaxAckRetries)
            {
                Serial.println("[HD][TX] END send failed after retries");
            }
            else
            {
                delay(50);
            }
            continue;
        }

        state.transition(ResearchEvent::TX_COMPLETE);
        endAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);
        char status[24];
        withRetrySuffix(status, sizeof(status), endAckOk ? "ACK_OK" : "ACK_TIMEOUT", retry);
        logAck(endTxTime, endAckOk, "END", status, -1, 0, retry);

        if (endAckOk)
        {
            state.transition(ResearchEvent::ACK_VALID);
            break;
        }

        if (retry < kMaxAckRetries)
        {
            state.transition(ResearchEvent::ACK_TIMEOUT);
            delay(50);
        }
        else
        {
            state.transition(ResearchEvent::RETRY_EXHAUSTED);
            Serial.println("[HD][TX] END ACK timeout after retries");
        }
    }

    StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
    StatusDisplay::setMessage(endAckOk ? "Transfer complete" : "Transfer done w/ END fail");

    g_session_id++;
    g_seq_num = 0;

    Serial.println("[HD][TX] Transfer complete\n");
    return endAckOk;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== LoRa Half-Duplex Node ===");

    StatusDisplay::init();

#if MESH_TX_BUTTON_ACTIVE_LOW
    pinMode(MESH_TX_BUTTON_PIN, INPUT_PULLUP);
#else
    pinMode(MESH_TX_BUTTON_PIN, INPUT);
#endif

    g_lastButtonSample = static_cast<uint8_t>(digitalRead(MESH_TX_BUTTON_PIN));
    g_stableButtonState = g_lastButtonSample;
    g_lastButtonChangeMs = millis();

    Serial.println("Initializing SD...");
    g_sd_ready = sdMgr.init();
    if (!g_sd_ready)
    {
        Serial.println("SD init failed; logging disabled");
    }
    else
    {
        Serial.println("SD ready; logging enabled");
    }
    StatusDisplay::setSD(g_sd_ready);

    Serial.println("Initializing LoRa...");
    g_session_id = static_cast<uint16_t>(millis() & 0xFFFF);
    g_seq_num = 0;
    const bool loraOk = lora.init(&g_session_id, &g_seq_num);

    StatusDisplay::setLoRa(loraOk ? StatusDisplay::LORA_OK_IDLE : StatusDisplay::LORA_FAIL);
    state.transition(ResearchEvent::SETUP_COMPLETE);

    Serial.printf("Session: 0x%04X\n", g_session_id);
    Serial.printf("Button pin=%d active=%s\n", MESH_TX_BUTTON_PIN, MESH_TX_BUTTON_ACTIVE_LOW ? "LOW" : "HIGH");
    Serial.printf("Run config: run_id=%s role=%s sf=%u bw=%.1f cr=4/%u timeout_ms=%lu tx_power=%d\n",
                  MESH_RUN_ID,
                  MESH_LOG_ROLE,
                  static_cast<unsigned>(MESH_LORA_SF),
                  static_cast<double>(MESH_LORA_BW_KHZ),
                  static_cast<unsigned>(MESH_LORA_CR),
                  static_cast<unsigned long>(timeout_ms),
                  static_cast<int>(MESH_LORA_TX_POWER_DBM));
    Serial.println("Node ready: idle RX, press button to TX payload\n");
    StatusDisplay::setMessage("Press button to TX");

    const uint32_t now = millis();
    g_lastIdleDisplayMs = now;
    g_lastIdleSerialMs = now;
}

void loop()
{
    processIncomingPacket();

    if (wasTxButtonPressed())
    {
        Serial.println("[HD] TX button pressed");
        StatusDisplay::setMessage("Button pressed");
        transmitPayloadFile();
        StatusDisplay::setMessage("Press button to TX");

        const uint32_t now = millis();
        g_lastIdleDisplayMs = now;
        g_lastIdleSerialMs = now;
    }
    else
    {
        publishIdleHeartbeat();
    }

    delay(10);
}
