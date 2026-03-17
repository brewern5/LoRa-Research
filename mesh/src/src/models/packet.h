#pragma once
#include <Arduino.h>
#include <stdint.h>

// Packet format for our audio file transfer
#define LORA_PROTOCOL_VERSION 1

// Defines what type of audio file is being sent
#define CODEC_RAW_PCM 0x00
#define CODEC_COMPRESSED 0x01

// LoRa protocol constraints
#define LORA_MAX_PAYLOAD 255
#define LORA_HEADER_SIZE 10
#define LORA_MAX_DATA_PAYLOAD (LORA_MAX_PAYLOAD - LORA_HEADER_SIZE)

// Packet type enums
#define PKT_AUDIO_START 0x01
#define PKT_AUDIO_DATA 0x02
#define PKT_AUDIO_END 0x03
#define PKT_ACK 0x04

// ACK status codes
#define ACK_STATUS_OK 0x00
#define ACK_STATUS_CRC_ERR 0x01
#define ACK_STATUS_MISSING 0x02


// ─── Structs ──────────────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct LoRaHeader {
  uint8_t ver_type;    // VERSION (high nibble) | TYPE (low nibble)
  uint8_t src_id;
  uint8_t dst_id;
  uint8_t exp_id;
  uint16_t session_id;
  uint16_t seq_num;
  uint8_t tx_pow;
  uint8_t sf_cr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AudioStartPayload{
  uint16_t total_frags;
  uint8_t codec_id;
  uint16_t sample_hz;
  uint16_t duration_ms;
  uint32_t total_size;
  uint16_t crc16;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AudioDataPayload{
  uint8_t data[LORA_MAX_DATA_PAYLOAD];
  uint8_t len;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AudioEndPayload{
  uint16_t frag_count;
  uint32_t crc32;
  uint8_t reserved;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct AckPayload{
  uint16_t ack_seq;
  uint8_t status;
};
#pragma pack(pop)

struct LoRaAudioPacket{
  LoRaHeader header;
  union {
    AudioStartPayload start;
    AudioDataPayload data;
    AudioEndPayload end;
    AckPayload ack;
    uint8_t raw[LORA_MAX_DATA_PAYLOAD];
  } payload;
};
#pragma pack(pop)


// ─── Nibble helpers (must be inline) ─────────────────────────────────────────

inline uint8_t makeVerType(uint8_t version, uint8_t type) {
  return ((version & 0x0F) << 4) | (type & 0x0F);
}
inline uint8_t getVersion(uint8_t ver_type) { return (ver_type >> 4) & 0x0F; }
inline uint8_t getType(uint8_t ver_type)    { return ver_type & 0x0F; }

inline uint8_t makeSFCR(uint8_t sf, uint8_t cr) {
  return ((sf & 0x0F) << 4) | (cr & 0x0F);
}
inline uint8_t getSF(uint8_t sf_cr)         { return (sf_cr >> 4) & 0x0F; }
inline uint8_t getCodingRate(uint8_t sf_cr) { return sf_cr & 0x0F; }


// ─── Function declarations (implemented in loraProtocol.cpp) ─────────────────

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
  uint8_t cr
);

uint16_t crc16(const uint8_t* data, size_t len);
uint32_t crc32(const uint8_t* data, size_t len);

void serializeHeader(const LoRaHeader* hdr, uint8_t* buf);
void deserializeHeader(const uint8_t* buf, LoRaHeader* hdr);
void serializeAudioStart(const AudioStartPayload* payload, uint8_t* buf);
void deserializeAudioStart(const uint8_t* buf, AudioStartPayload* payload);
void serializeAudioEnd(const AudioEndPayload* payload, uint8_t* buf);
void deserializeAudioEnd(const uint8_t* buf, AudioEndPayload* payload);

#ifdef LORA_DEBUG
void printHeader(const LoRaHeader* hdr);
void printAudioStart(const AudioStartPayload* p);
#endif