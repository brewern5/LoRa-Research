#include <Arduino.h>
#include <RadioLib.h>

#include "src/models/packet.h"
#include "src/storage/SdManager.h"
#include "src/comms/LoraManager.h"
#include "src/display/StatusDisplay.h"

SdManager sdMgr;
AudioPacket packet;
bool g_sd_ready = false;

LoRaManager lora;

// Session
uint16_t g_session_id;
uint16_t g_seq_num;
uint32_t timeout_ms = 2000;

// Display


// Placeholder location until GPS integration is added
constexpr float kDefaultLat = 0.0f;
constexpr float kDefaultLon = 0.0f;

// ──────────────────────────────────────────────
//  Demo audio data (replace with real source)
// ──────────────────────────────────────────────
// 512 bytes of fake audio to demonstrate fragmentation
static const uint8_t DEMO_AUDIO[512] = { /* fill with real PCM bytes */ 0 };
static const size_t  DEMO_AUDIO_LEN  = sizeof(DEMO_AUDIO);


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== LoRa Audio Transmitter ===");

  Serial.println("Initializing Display...");
  StatusDisplay::init();

  Serial.println("Initializing SD...");

  g_sd_ready = sdMgr.init();
  delay(2000);
  if (!g_sd_ready) {
    Serial.println("SD init failed; logging disabled");
  } else {
    Serial.println("SD ready; logging to lora_log.csv");
  }
  StatusDisplay::setSD(g_sd_ready);


  Serial.println("Initializing LoRa...");
  // Generate a simple session ID from millis() — replace with something
  // more robust (e.g. random32, incrementing NVS counter) in production
  g_session_id = (uint16_t)(millis() & 0xFFFF);
  g_seq_num = 0;
  bool loraOk = lora.init(&g_session_id, &g_seq_num);

  StatusDisplay::setLoRa(loraOk ? StatusDisplay::LORA_OK_IDLE : StatusDisplay::LORA_FAIL);

  Serial.printf("Session: 0x%04X\n", g_session_id);
  Serial.println("LoRa ready\n");
  
}


void loop() {

  auto logAck = [&](uint32_t txTimeMs, bool ackOk, const char* packetType, const char* status,
                    int16_t fragIndex, uint16_t fragLen) {
    if (!g_sd_ready) return;
    const uint32_t ackTimeMs = millis();
    const int rssi = ackOk ? static_cast<int>(lora.getLastRSSI()) : 0;
    const float snr = ackOk ? lora.getLastSNR() : 0.0f;
    const uint16_t seqNum = lora.getLastSeqNum();
    Serial.printf("[LOG] write csv row type=%s status=%s tx=%lu ack=%lu rssi=%d snr=%.1f\n",
                  packetType, status, txTimeMs, ackTimeMs, rssi, snr);

    delay(200);
    const bool logged = sdMgr.logTransmission(kDefaultLat, kDefaultLon, txTimeMs, ackTimeMs, rssi, snr,
                                              g_session_id, seqNum, fragIndex, fragLen, packetType, status);
    if (!logged) {
      Serial.println("[LOG] SD row persist failed");
    }
  };

  // ── Calculate fragmentation ──────────────────
  uint16_t total_frags = (DEMO_AUDIO_LEN + LORA_MAX_DATA_PAYLOAD - 1)
                         / LORA_MAX_DATA_PAYLOAD;

  uint32_t audio_crc = crc32(DEMO_AUDIO, DEMO_AUDIO_LEN);

  Serial.printf("Starting transfer: %u bytes, %u fragments, CRC32=0x%08X\n",
                DEMO_AUDIO_LEN, total_frags, audio_crc);

  // ── 1. Send AUDIO_START ──────────────────────
  uint32_t startTxTime = millis();
  if (!lora.sendAudioStart(total_frags, 0x00 /*raw PCM*/, 8000, 64, DEMO_AUDIO_LEN)) {
    Serial.println("START failed — retrying in 5s");
    delay(200);
    logAck(startTxTime, false, "START", "TX_FAIL", -1, 0);
    delay(5000);
    return;
  }
  // Optional: wait for ACK on START
  bool startAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);

  delay(200);
  logAck(startTxTime, startAckOk, "START", startAckOk ? "ACK_OK" : "ACK_TIMEOUT", -1, 0);
  delay(200);

  // ── 2. Send DATA fragments ───────────────────
  size_t offset = 0;
  uint16_t dataOkCount = 0;
  uint16_t dataFailCount = 0;
  for (uint16_t frag = 0; frag < total_frags; frag++) {
    size_t chunk = min((size_t)LORA_MAX_DATA_PAYLOAD, DEMO_AUDIO_LEN - offset);
    const uint32_t dataTxTime = millis();
    bool dataSent = lora.sendAudioData(DEMO_AUDIO + offset, (uint8_t)chunk);

    if (!dataSent) {
      Serial.printf("DATA frag %u failed\n", frag);
      // In production: implement retransmit or NACK handling here
      StatusDisplay::setMessage("Failed Sending Data Frag!");
      dataFailCount++;

      delay(200);
      logAck(dataTxTime, false, "DATA", "TX_FAIL", static_cast<int16_t>(frag), static_cast<uint16_t>(chunk));
      delay(200);

      offset += chunk;
      delay(50); // Small inter-packet gap; tune to your duty cycle / SF
      continue;
    }

    bool dataAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);
    if (dataAckOk) {
      dataOkCount++;
    } else {
      dataFailCount++;
      Serial.printf("DATA frag %u no ACK\n", frag);
      StatusDisplay::setMessage("Data Frag ACK Timeout!");
    }
    delay(200);
        logAck(dataTxTime, dataAckOk, "DATA", dataAckOk ? "ACK_OK" : "ACK_TIMEOUT",
          static_cast<int16_t>(frag), static_cast<uint16_t>(chunk));

    StatusDisplay::onPacketSent();
    offset += chunk;
    delay(50); // Small inter-packet gap; tune to your duty cycle / SF
  }

  Serial.printf("DATA summary: acked=%u failed_or_timeout=%u total=%u\n",
                dataOkCount, dataFailCount, total_frags);

  // ── 3. Send AUDIO_END ────────────────────────
  uint32_t endTxTime = millis();
  bool endSentOk = lora.sendAudioEnd(total_frags, audio_crc);
  bool endAckOk = endSentOk ? lora.waitForAck(lora.getLastSeqNum(), timeout_ms) : false;
  delay(200);
  logAck(endTxTime, endAckOk, "END", endSentOk ? (endAckOk ? "ACK_OK" : "ACK_TIMEOUT") : "TX_FAIL", -1, 0);
  delay(200);

  // ── Bump session for next run ────────────────
  g_session_id++;
  g_seq_num = 0;

  Serial.println("Transfer complete. Waiting 10s...\n");
  delay(10000);
}