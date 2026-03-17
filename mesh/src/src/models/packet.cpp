#include "packet.h"

// ─── Header builder ───────────────────────────────────────────────────────────

void buildHeader(
  LoRaHeader* hdr,
  uint8_t type,
  uint8_t src,
  uint8_t dst,
  uint8_t exp_id,
  uint16_t session,
  uint16_t seq,
  uint8_t tx_pow,
  uint8_t sf,
  uint8_t cr)
{
  hdr->ver_type = makeVerType(LORA_PROTOCOL_VERSION, type);
  hdr->src_id = src;
  hdr->dst_id = dst;
  hdr->exp_id = exp_id;
  hdr->session_id = session;
  hdr->seq_num = seq;
  hdr->tx_pow = tx_pow;
  hdr->sf_cr = makeSFCR(sf, cr);
}


// ─── CRC ─────────────────────────────────────────────────────────────────────

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
  }
  return crc;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
    }
  }
  return ~crc;
}


// ─── Serialization ────────────────────────────────────────────────────────────

void serializeHeader(const LoRaHeader* hdr, uint8_t* buf) {
  memcpy(buf, hdr, LORA_HEADER_SIZE);
}

void deserializeHeader(const uint8_t* buf, LoRaHeader* hdr) {
  memcpy(hdr, buf, LORA_HEADER_SIZE);
}

void serializeAudioStart(const AudioStartPayload* payload, uint8_t* buf) {
  memcpy(buf, payload, sizeof(AudioStartPayload));
}

void deserializeAudioStart(const uint8_t* buf, AudioStartPayload* payload) {
  memcpy(payload, buf, sizeof(AudioStartPayload));
}

void serializeAudioEnd(const AudioEndPayload* payload, uint8_t* buf) {
  memcpy(buf, payload, sizeof(AudioEndPayload));
}

void deserializeAudioEnd(const uint8_t* buf, AudioEndPayload* payload) {
  memcpy(payload, buf, sizeof(AudioEndPayload));
}


// ─── Debug printing ───────────────────────────────────────────────────────────

#ifdef LORA_DEBUG
void printHeader(const LoRaHeader* hdr) {
  Serial.println(F("--- LoRa Header ---"));
  Serial.printf("  Version    : %d\n",       getVersion(hdr->ver_type));
  Serial.printf("  Type       : 0x%02X\n",   getType(hdr->ver_type));
  Serial.printf("  SRC        : 0x%02X\n",   hdr->src_id);
  Serial.printf("  DST        : 0x%02X\n",   hdr->dst_id);
  Serial.printf("  Experiment : %d\n",        hdr->exp_id);
  Serial.printf("  Session    : 0x%04X\n",   hdr->session_id);
  Serial.printf("  Seq Num    : %d\n",        hdr->seq_num);
  Serial.printf("  TX Power   : %d dBm\n",   hdr->tx_pow);
  Serial.printf("  SF         : %d\n",        getSF(hdr->sf_cr));
  Serial.printf("  CR         : 4/%d\n",      getCodingRate(hdr->sf_cr));  // fixed: was getCR()
  Serial.println(F("-------------------"));
}

void printAudioStart(const AudioStartPayload* p) {
  Serial.println(F("--- Audio Start ---"));
  Serial.printf("  Total Frags: %d\n",       p->total_frags);
  Serial.printf("  Codec      : 0x%02X (%s)\n", p->codec_id,
                p->codec_id == CODEC_RAW_PCM ? "Raw PCM" : "Compressed");
  Serial.printf("  Sample Rate: %d Hz\n",    p->sample_hz);
  Serial.printf("  Duration   : %d ms\n",    p->duration_ms);
  Serial.printf("  Total Size : %lu bytes\n", p->total_size);
  Serial.printf("  CRC16      : 0x%04X\n",   p->crc16);
  Serial.println(F("-------------------"));
}
#endif // LORA_DEBUG