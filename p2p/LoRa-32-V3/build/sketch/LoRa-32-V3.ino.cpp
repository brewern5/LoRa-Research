#include <Arduino.h>
#line 1 "C:\\Users\\bobjo\\OneDrive\\Documents\\Arduino\\LoRa-32-V3\\LoRa-32-V3.ino"
#include <RadioLib.h>
#include "packet.h"
#include "sd_card.h"
#include "LoRa.h"

// SD Card manager
sdManager sdMgr;
AudioPacket packet;

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

  Serial.println("Initializing LoRa...");

  int state = radio.begin(TX_FREQ_MHZ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init failed, code %d — halting\n", state);
    while (true);
  }

  radio.setSpreadingFactor(TX_SF);
  radio.setBandwidth(TX_BW_KHZ);
  radio.setCodingRate(TX_CR);
  radio.setOutputPower(TX_POWER_DBM);

  // Generate a simple session ID from millis() — replace with something
  // more robust (e.g. random32, incrementing NVS counter) in production
  g_session_id = (uint16_t)(millis() & 0xFFFF);
  g_seq_num    = 0;

  Serial.printf("Session: 0x%04X\n", g_session_id);
  Serial.println("LoRa ready\n");
}


void loop() {

  // ── Calculate fragmentation ──────────────────
  uint16_t total_frags = (DEMO_AUDIO_LEN + LORA_MAX_DATA_PAYLOAD - 1)
                         / LORA_MAX_DATA_PAYLOAD;

  uint32_t audio_crc = crc32(DEMO_AUDIO, DEMO_AUDIO_LEN);

  Serial.printf("Starting transfer: %u bytes, %u fragments, CRC32=0x%08X\n",
                DEMO_AUDIO_LEN, total_frags, audio_crc);

  // ── 1. Send AUDIO_START ──────────────────────
  if (!sendAudioStart(total_frags, 0x00 /*raw PCM*/, 8000, 64, DEMO_AUDIO_LEN)) {
    Serial.println("START failed — retrying in 5s");
    delay(5000);
    return;
  }
  // Optional: wait for ACK on START
  waitForAck(g_seq_num - 1);

  // ── 2. Send DATA fragments ───────────────────
  size_t offset = 0;
  for (uint16_t frag = 0; frag < total_frags; frag++) {
    size_t chunk = min((size_t)LORA_MAX_DATA_PAYLOAD, DEMO_AUDIO_LEN - offset);
    if (!sendAudioData(DEMO_AUDIO + offset, (uint8_t)chunk)) {
      Serial.printf("DATA frag %u failed\n", frag);
      // In production: implement retransmit or NACK handling here
    }
    offset += chunk;
    delay(50); // Small inter-packet gap; tune to your duty cycle / SF
  }

  // ── 3. Send AUDIO_END ────────────────────────
  sendAudioEnd(total_frags, audio_crc);
  waitForAck(g_seq_num - 1);

  // ── Bump session for next run ────────────────
  g_session_id++;
  g_seq_num = 0;

  Serial.println("Transfer complete. Waiting 10s...\n");
  delay(10000);
}
