#include <Arduino.h>
#include <RadioLib.h>

#include "../src/models/packet.h"
#include "../src/storage/SdManager.h"
#include "../src/comms/LoraManager.h"
#include "../src/display/StatusDisplay.h"

SdManager sdMgr;
bool g_sd_ready = false;

LoRaManager lora;

// Session
uint16_t g_session_id;
uint16_t g_seq_num;
uint32_t timeout_ms = 2000;

// Placeholder location until GPS integration is added
constexpr float kDefaultLat = 0.0f;
constexpr float kDefaultLon = 0.0f;

constexpr const char* kPayloadFile = "lora_payload.bin";
constexpr uint8_t kPayloadCodec = CODEC_RAW_PCM;
constexpr uint16_t kPayloadSampleHz = 8000;
constexpr uint16_t kPayloadDurationMs = 0;

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
                      int16_t fragIndex, uint16_t fragLen)
    {
        if (!g_sd_ready)
            return;
        const uint32_t ackTimeMs = millis();
        const int rssi = ackOk ? static_cast<int>(lora.getLastRSSI()) : 0;
        const float snr = ackOk ? lora.getLastSNR() : 0.0f;
        const uint16_t seqNum = lora.getLastSeqNum();
        Serial.printf("[LOG] write csv row type=%s status=%s tx=%lu ack=%lu rssi=%d snr=%.1f\n",
                      packetType, status, txTimeMs, ackTimeMs, rssi, snr);

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
    uint32_t startTxTime = millis();
    if (!lora.sendAudioStart(total_frags, kPayloadCodec, kPayloadSampleHz, kPayloadDurationMs, payloadSize))
    {
        Serial.println("START failed — retrying in 5s");
        delay(200);
        logAck(startTxTime, false, "START", "TX_FAIL", -1, 0);
        sdMgr.closeAudioFile();
        StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
        delay(5000);
        return;
    }
    bool startAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);

    delay(200);
    logAck(startTxTime, startAckOk, "START", startAckOk ? "ACK_OK" : "ACK_TIMEOUT", -1, 0);
    delay(200);

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
        const uint32_t dataTxTime = millis();
        bool dataSent = lora.sendAudioData(packet.buffer, static_cast<uint8_t>(chunk));

        if (!dataSent)
        {
            Serial.printf("DATA frag %u failed\n", frag);
            StatusDisplay::setMessage("Failed Sending Data Frag!");
            dataFailCount++;

            delay(200);
            logAck(dataTxTime, false, "DATA", "TX_FAIL", static_cast<int16_t>(frag), static_cast<uint16_t>(chunk));
            delay(200);

            frag++;
            delay(50);
            continue;
        }

        bool dataAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);
        if (dataAckOk)
        {
            dataOkCount++;
        }
        else
        {
            dataFailCount++;
            Serial.printf("DATA frag %u no ACK\n", frag);
            StatusDisplay::setMessage("Data Frag ACK Timeout!");
        }
        delay(200);
        logAck(dataTxTime, dataAckOk, "DATA", dataAckOk ? "ACK_OK" : "ACK_TIMEOUT",
               static_cast<int16_t>(frag), static_cast<uint16_t>(chunk));

        StatusDisplay::onPacketSent();
        frag++;
        delay(50);
    }

    sdMgr.closeAudioFile();

    Serial.printf("DATA summary: acked=%u failed_or_timeout=%u total=%u\n",
                  dataOkCount, dataFailCount, frag);
                  
    // ── 3. Send AUDIO_END ────────────────────────
    uint32_t endTxTime = millis();
    bool endSentOk = lora.sendAudioEnd(frag, audio_crc);
    bool endAckOk = endSentOk ? lora.waitForAck(lora.getLastSeqNum(), timeout_ms) : false;
    delay(200);
    logAck(endTxTime, endAckOk, "END", endSentOk ? (endAckOk ? "ACK_OK" : "ACK_TIMEOUT") : "TX_FAIL", -1, 0);

    // ── Bump session for next run ────────────────
    StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
    StatusDisplay::setMessage("Transfer complete");
    g_session_id++;
    g_seq_num = 0;

    Serial.println("Transfer complete. Waiting 10s...\n");
    delay(10000);
}
