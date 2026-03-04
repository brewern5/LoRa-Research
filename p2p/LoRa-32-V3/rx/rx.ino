#include <Arduino.h>

#include "../src/models/packet.h"
#include "../src/storage/SdManager.h"
#include "../src/comms/LoraManager.h"
#include "../src/display/StatusDisplay.h"

SdManager sdMgr;
bool g_sd_ready = false;
LoRaManager lora;

uint16_t g_session_id = 0;
uint16_t g_seq_num = 0;

constexpr float kDefaultLat = 0.0f;
constexpr float kDefaultLon = 0.0f;

static const char* packetTypeLabel(uint8_t packetType) {
  switch (packetType) {
    case PKT_AUDIO_START: return "START";
    case PKT_AUDIO_DATA:  return "DATA";
    case PKT_AUDIO_END:   return "END";
    case PKT_ACK:         return "ACK";
    default:              return "UNKNOWN";
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== LoRa Audio Receiver ===");

  Serial.println("Initializing Display...");
  StatusDisplay::init();

  Serial.println("Initializing SD...");
  g_sd_ready = sdMgr.init();
  delay(500);
  StatusDisplay::setSD(g_sd_ready);

  if (!g_sd_ready) {
    Serial.println("SD init failed; logging disabled");
  } else {
    Serial.println("SD ready; RX logging enabled");
  }

  Serial.println("Initializing LoRa...");
  bool loraOk = lora.init(&g_session_id, &g_seq_num);
  StatusDisplay::setLoRa(loraOk ? StatusDisplay::LORA_OK_IDLE : StatusDisplay::LORA_FAIL);
  Serial.println(loraOk ? "LoRa RX ready\n" : "LoRa RX init failed\n");
}

void loop() {
  uint8_t raw[LORA_MAX_PAYLOAD] = {0};
  size_t receivedLen = 0;

  if (!lora.receiveRaw(raw, sizeof(raw), &receivedLen)) {
    delay(20);
    return;
  }

  StatusDisplay::setLoRa(StatusDisplay::LORA_RECEIVING);

  const uint32_t rxTimeMs = millis();
  const int rssi = static_cast<int>(lora.getLastRSSI());
  const float snr = lora.getLastSNR();

  if (receivedLen < LORA_HEADER_SIZE) {
    Serial.printf("[RX] Dropped short packet len=%u\n", static_cast<unsigned>(receivedLen));
    return;
  }

  LoRaHeader hdr;
  deserializeHeader(raw, &hdr);
  const uint8_t packetType = getType(hdr.ver_type);
  const char* packetTypeStr = packetTypeLabel(packetType);

  int16_t fragIndex = -1;
  uint16_t fragLen = 0;
  if (packetType == PKT_AUDIO_DATA) {
    fragIndex = static_cast<int16_t>(hdr.seq_num);
    fragLen = static_cast<uint16_t>(receivedLen - LORA_HEADER_SIZE);
  }

  Serial.printf("[RX] type=%s seq=%u sess=0x%04X len=%u RSSI=%d SNR=%.1f\n",
                packetTypeStr,
                hdr.seq_num,
                hdr.session_id,
                static_cast<unsigned>(receivedLen),
                rssi,
                snr);

  StatusDisplay::onPacketReceived();

  if (g_sd_ready) {
    sdMgr.logTransmission(kDefaultLat, kDefaultLon, rxTimeMs, rxTimeMs, rssi, snr,
                          hdr.session_id, hdr.seq_num, fragIndex, fragLen,
                          packetTypeStr, "RX_RECV");
  }

  const uint32_t ackTimeMs = millis();
  bool ackSent = lora.sendAckFor(hdr, ACK_STATUS_OK);
  if (g_sd_ready) {
    sdMgr.logTransmission(kDefaultLat, kDefaultLon, rxTimeMs, ackTimeMs, rssi, snr,
                          hdr.session_id, hdr.seq_num, fragIndex, fragLen,
                          packetTypeStr, ackSent ? "ACK_SENT" : "ACK_SEND_FAIL");
  }

  StatusDisplay::setLoRa(StatusDisplay::LORA_OK_IDLE);
}
