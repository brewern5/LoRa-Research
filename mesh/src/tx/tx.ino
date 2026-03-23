#include <Arduino.h>
#include <RadioLib.h>

#include "../src/models/packet.h"
#include "../src/storage/SdManager.h"
#include "../src/comms/LoraManager.h"
#include "../src/app/ResearchStateMachine.h"
#include "../src/display/StatusDisplay.h"

SdManager sdMgr;
bool g_sd_ready = false;

LoRaManager lora;
ResearchStateMachine state("TX");

// Session
uint16_t g_session_id;
uint16_t g_seq_num;
uint32_t timeout_ms = 2000;

// Placeholder location until GPS integration is added
constexpr float kDefaultLat = 0.0f;
constexpr float kDefaultLon = 0.0f;

constexpr const char* kPayloadFile = "lora_payload_new.bin";
constexpr uint8_t kPayloadCodec = CODEC_RAW_PCM;
constexpr uint16_t kPayloadSampleHz = 8000;
constexpr uint16_t kPayloadDurationMs = 0;
constexpr uint8_t kMaxAckRetries = 2;

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

static bool getPayloadStats(SdManager& manager, const char* filename,
                            uint32_t& totalSize, uint16_t& totalFrags, uint32_t& payloadCrc)
{
    totalSize = 0;
    totalFrags = 0;
    payloadCrc = 0;

    if (!manager.openAudioFile(filename))
    {
        Serial.printf("[TX] Unable to open payload file: %s\n", filename);
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
        Serial.println("[TX] Payload file is empty");
        return false;
    }

    if (fragCount > 0xFFFF)
    {
        Serial.printf("[TX] Payload too large, fragments=%lu exceeds protocol max\n", static_cast<unsigned long>(fragCount));
        return false;
    }

    totalFrags = static_cast<uint16_t>(fragCount);
    payloadCrc = ~crcState;
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== LoRa Audio Transmitter ===");

    Serial.println("Initializing Display...");
    StatusDisplay::init();

    Serial.println("Initializing SD...");

    g_sd_ready = sdMgr.init();
    delay(2000);
    if (!g_sd_ready)
    {
        Serial.println("SD init failed; logging disabled");
    }
    else
    {
        Serial.println("SD ready; logging to lora_log.csv");
    }
    StatusDisplay::setSD(g_sd_ready);

    Serial.println("Initializing LoRa...");
    g_session_id = (uint16_t)(millis() & 0xFFFF);
    g_seq_num = 0;
    bool loraOk = lora.init(&g_session_id, &g_seq_num);

    StatusDisplay::setLoRa(loraOk ? StatusDisplay::LORA_OK_IDLE : StatusDisplay::LORA_FAIL);
    state.transition(ResearchEvent::SETUP_COMPLETE);

    Serial.printf("Session: 0x%04X\n", g_session_id);
    Serial.println("LoRa ready\n");
}

void loop()
{

    if (!g_sd_ready)
    {
        Serial.println("SD not ready; cannot transmit payload file");
        StatusDisplay::setMessage("SD not ready");
        delay(2000);
        return;
    }

    auto logAck = [&](uint32_t txTimeMs, bool ackOk, const char *packetType, const char *status,
                      int16_t fragIndex, uint16_t fragLen, uint8_t retryIndex)
    {
        if (!g_sd_ready)
            return;
        const uint32_t ackTimeMs = millis();
        const int rssi = ackOk ? static_cast<int>(lora.getLastRSSI()) : 0;
        const float snr = ackOk ? lora.getLastSNR() : 0.0f;
        const uint16_t seqNum = lora.getLastSeqNum();
        Serial.printf("[LOG] write csv row type=%s status=%s retry=%u tx=%lu ack=%lu rssi=%d snr=%.1f\n",
                      packetType, status, static_cast<unsigned>(retryIndex), txTimeMs, ackTimeMs, rssi, snr);

        delay(200);
        const bool logged = sdMgr.logTransmission(kDefaultLat, kDefaultLon, txTimeMs, ackTimeMs, rssi, snr,
                                                  g_session_id, seqNum, fragIndex, fragLen, packetType, status);
        if (!logged)
        {
            Serial.println("[LOG] SD row persist failed");
        }
    };

    uint32_t payloadSize = 0;
    uint16_t total_frags = 0;
    uint32_t audio_crc = 0;
    if (!getPayloadStats(sdMgr, kPayloadFile, payloadSize, total_frags, audio_crc))
    {
        StatusDisplay::setMessage("Missing/invalid payload");
        delay(3000);
        return;
    }

    if (!sdMgr.openAudioFile(kPayloadFile))
    {
        Serial.printf("[TX] Failed to reopen payload file for transmit: %s\n", kPayloadFile);
        StatusDisplay::setMessage("Payload open failed");
        delay(3000);
        return;
    }

    Serial.printf("Starting transfer from %s: %lu bytes, %u fragments, CRC32=0x%08lX\n",
                  kPayloadFile,
                  static_cast<unsigned long>(payloadSize),
                  total_frags,
                  static_cast<unsigned long>(audio_crc));
    StatusDisplay::setLoRa(StatusDisplay::LORA_TRANSMITTING);
    StatusDisplay::setMessage("Transmitting payload...");

    // ── 1. Send AUDIO_START ──────────────────────
    bool startAckOk = false;
    for (uint8_t retry = 0; retry <= kMaxAckRetries; ++retry)
    {
        state.transition(ResearchEvent::PAYLOAD_AVAILABLE);
        uint32_t startTxTime = millis();
        bool startSent = lora.sendAudioStart(total_frags, kPayloadCodec, kPayloadSampleHz, kPayloadDurationMs, payloadSize);

        if (!startSent)
        {
            state.transition(ResearchEvent::TX_FAILED);
            char status[24];
            withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
            delay(200);
            logAck(startTxTime, false, "START", status, -1, 0, retry);

            if (retry == kMaxAckRetries)
            {
                Serial.println("START failed after retries");
                sdMgr.closeAudioFile();
                StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
                delay(3000);
                return;
            }
            delay(50);
            continue;
        }

        state.transition(ResearchEvent::TX_COMPLETE);
        startAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);

        char status[24];
        withRetrySuffix(status, sizeof(status), startAckOk ? "ACK_OK" : "ACK_TIMEOUT", retry);
        delay(200);
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
        delay(3000);
        return;
    }

    // ── 2. Send DATA fragments ───────────────────
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
            bool dataSent = lora.sendAudioData(packet.buffer, static_cast<uint8_t>(chunk));

            if (!dataSent)
            {
                state.transition(ResearchEvent::TX_FAILED);
                char status[24];
                withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
                delay(200);
                logAck(dataTxTime, false, "DATA", status,
                       static_cast<int16_t>(frag), static_cast<uint16_t>(chunk), retry);

                if (retry == kMaxAckRetries)
                {
                    dataFailCount++;
                    Serial.printf("DATA frag %u failed after retries\n", frag);
                    StatusDisplay::setMessage("Failed Sending Data Frag!");
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
            delay(200);
            logAck(dataTxTime, dataAckOk, "DATA", status,
                   static_cast<int16_t>(frag), static_cast<uint16_t>(chunk), retry);

            if (dataAckOk)
            {
                state.transition(ResearchEvent::ACK_VALID);
                dataOkCount++;
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
                Serial.printf("DATA frag %u no ACK after retries\n", frag);
                StatusDisplay::setMessage("Data Frag ACK Timeout!");
            }
        }

        if (dataAckOk)
        {
            StatusDisplay::onPacketSent();
        }
        frag++;
        delay(50);
    }

    sdMgr.closeAudioFile();

    Serial.printf("DATA summary: acked=%u failed_or_timeout=%u total=%u\n",
                  dataOkCount, dataFailCount, frag);
                  
    // ── 3. Send AUDIO_END ────────────────────────
    bool endAckOk = false;
    for (uint8_t retry = 0; retry <= kMaxAckRetries; ++retry)
    {
        state.transition(ResearchEvent::PAYLOAD_AVAILABLE);
        uint32_t endTxTime = millis();
        bool endSentOk = lora.sendAudioEnd(frag, audio_crc);

        if (!endSentOk)
        {
            state.transition(ResearchEvent::TX_FAILED);
            char status[24];
            withRetrySuffix(status, sizeof(status), "TX_FAIL", retry);
            delay(200);
            logAck(endTxTime, false, "END", status, -1, 0, retry);

            if (retry == kMaxAckRetries)
            {
                Serial.println("END send failed after retries");
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
        delay(200);
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
            Serial.println("END ACK timeout after retries");
        }
    }

    // ── Bump session for next run ────────────────
    StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
    StatusDisplay::setMessage("Transfer complete");
    g_session_id++;
    g_seq_num = 0;

    Serial.println("Transfer complete. Waiting 10s...\n");
    delay(10000);
}
