#include <RadioLib.h>
#include "src/models/packet.h"
#include "src/storage/SdManager.h"
#include "src/comms/LoraManager.h"

// SD Card manager
SdManager sdMgr;
AudioPacket packet;
bool g_sd_ready = false;

LoRaManager lora;

// Session
uint16_t g_session_id;
uint16_t g_seq_num;
uint32_t timeout_ms = 2000;

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

  Serial.println("Initializing SD...");

  
  g_sd_ready = sdMgr.init();
  delay(2000);
  if (!g_sd_ready) {
    Serial.println("SD init failed; logging disabled");
  // } else {
  //   Serial.println("SD ready; logging to lora_log.csv");
  }
  

  Serial.println("Initializing LoRa...");

  // Generate a simple session ID from millis() — replace with something
  // more robust (e.g. random32, incrementing NVS counter) in production
  g_session_id = (uint16_t)(millis() & 0xFFFF);
  g_seq_num = 0;

  lora.init(&g_session_id, &g_seq_num);

  Serial.printf("Session: 0x%04X\n", g_session_id);
  Serial.println("LoRa ready\n");
}


void loop() {

  auto logAck = [&](uint32_t txTimeMs, bool ackOk) {
    if (!g_sd_ready) return;
    const uint32_t ackTimeMs = millis();
    const int rssi = ackOk ? static_cast<int>(lora.getLastRSSI()) : 0;
    const float snr = ackOk ? lora.getLastSNR() : 0.0f;
    sdMgr.logTransmission(kDefaultLat, kDefaultLon, txTimeMs, ackTimeMs, rssi, snr);
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
    delay(5000);
    return;
  }
  // Optional: wait for ACK on START
  bool startAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);
  logAck(startTxTime, startAckOk);

  // ── 2. Send DATA fragments ───────────────────
  size_t offset = 0;
  for (uint16_t frag = 0; frag < total_frags; frag++) {
    size_t chunk = min((size_t)LORA_MAX_DATA_PAYLOAD, DEMO_AUDIO_LEN - offset);
    if (!lora.sendAudioData(DEMO_AUDIO + offset, (uint8_t)chunk)) {
      Serial.printf("DATA frag %u failed\n", frag);
      // In production: implement retransmit or NACK handling here
    }
    offset += chunk;
    delay(50); // Small inter-packet gap; tune to your duty cycle / SF
  }

  // ── 3. Send AUDIO_END ────────────────────────
  uint32_t endTxTime = millis();
  lora.sendAudioEnd(total_frags, audio_crc);
  bool endAckOk = lora.waitForAck(lora.getLastSeqNum(), timeout_ms);
  logAck(endTxTime, endAckOk);

  // ── Bump session for next run ────────────────
  g_session_id++;
  g_seq_num = 0;

  Serial.println("Transfer complete. Waiting 10s...\n");
  delay(10000);
}