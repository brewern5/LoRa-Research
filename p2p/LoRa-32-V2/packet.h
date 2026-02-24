#pragma once
#include <Arduino.h>

// Packet format for our audio file transfer
#define LORA_PROTOCOL_VERSION 1

// Defines what type of audio file is being sent
#define CODEC_RAW_PCM    0x00
#define CODEC_COMPRESSED 0x01

// Lora procotol constraints
#define LORA_MAX_PAYLOAD 255 // SX1262 max LoRa payload (bytes)
#define LORA_HEADER_SIZE 10 // Fixed header size (bytes)
#define LORA_MAX_DATA_PAYLOAD (LORA_MAX_PAYLOAD - LORA_HEADER_SIZE)  // 245 bytes

// Packet enums
#define PKT_AUDIO_START 0x01 
#define PKT_AUDIO_DATA 0x02
#define PKT_AUDIO_END 0x03
#define PKT_ACK 0x04


/*
#    HEADER  -  10 bytes
*/
typedef struct __attribute__((packed)) {
  uint8_t ver_type;  // VERSION (high nibble) | TYPE (low nibble) - Using nibbles to avoid using more than 1 byte on the version
  uint8_t src_id;
  uint8_t dst_id;
  uint8_t exp_id;       // Experiment run ID
  uint16_t session_id;  // Unique session identifier
  uint16_t seq_num;     // Fragment sequence number
  uint8_t tx_pow;       // TX power in dBm
  uint8_t sf_cr;        // Spreading factor | coding rate
} LoRaHeader;


/*
#   PKT_AUDIO_START packet  -  10 bytes
*/
typedef struct __attribute__((packed)) {
  uint16_t total_frags;
  uint8_t codec_id;      // raw or compressed audio file we are sending. 0x00 for raw, 0x01 for compressed
  uint16_t sample_hz;    // what the original sample rate of the file we are sending is
  uint16_t duration_ms;  // How long the file is
  uint32_t total_size;
  uint16_t crc16;  // Cyclic Redundancy Chec
} AudioStartPayload;


/*
#   PKT_AUDIO_DATA payload  -  7 bytes 
*/
typedef struct __attribute__((packed)) {
  uint8_t data[LORA_MAX_DATA_PAYLOAD];  // Raw data buffer - max of LORA_MAX_DATA_PAYLOAD
  uint8_t len;                          // Actual number of valid bytes in data[]
} AudioDataPayload;


/*
#   PKT_AUDIO_END  -  7 bytes 
*/
typedef struct __attribute__((packed)) {
  uint16_t frag_count;
  uint32_t crc32;
  uint8_t reserved;
} AudioEndPayload;


/*
#   ACK  -  3 bytes
*/
typedef struct __attribute__((packed)) {
  uint16_t ack_seq;
  uint8_t status;
} AckPayload;

#define ACK_STATUS_OK 0x00
#define ACK_STATUS_CRC_ERR 0x01
#define ACK_STATUS_MISSING 0x02


/*
#   Full packet structure 
*/
typedef struct __attribute__((packed)) {
  LoRaHeader header;
  union {
    AudioStartPayload start;
    AudioDataPayload data;
    AudioEndPayload end;
    AckPayload ack;
    uint8_t raw[LORA_MAX_DATA_PAYLOAD];
  } payload;
} LoRaAudioPacket;


/*
#   Helper methods
*/


inline uint8_t makeVerType(uint8_t version, uint8_t type) {
  return ((version & 0x0F) << 4) | (type & 0x0F);
}

inline uint8_t getVersion(uint8_t ver_type) {
  return (ver_type >> 4) & 0x0F;
}


inline uint8_t getType(uint8_t ver_type) {
  return ver_type & 0x0F;
}

/**
 * @brief Build sf_cr byte from spreading factor and coding rate.
 * @param sf  Spreading factor (7–12)
 * @param cr  Coding rate denominator (5–8, meaning 4/5 to 4/8)
 */
inline uint8_t makeSFCR(uint8_t sf, uint8_t cr) {
  return ((sf & 0x0F) << 4) | (cr & 0x0F);
}

inline uint8_t getSF(uint8_t sf_cr) {
  return (sf_cr >> 4) & 0x0F;
}


inline uint8_t getCodingRate(uint8_t sf_cr) {
  return sf_cr & 0x0F;
}


/*
#   Populate the header
*/
/**
 * @brief Fill a LoRaHeader with common fields.
 * @param hdr Pointer to header struct to populate
 * @param type Packet type (PKT_AUDIO_START, etc.)
 * @param src Source node ID
 * @param dst Destination node ID
 * @param exp_id Experiment run ID
 * @param session Session ID
 * @param seq Sequence number
 * @param tx_pow TX power in dBm
 * @param sf Spreading factor
 * @param cr Coding rate
 */
inline void buildHeader(
  LoRaHeader* hdr,
  uint8_t  type,
  uint8_t  src,
  uint8_t  dst,
  uint8_t  exp_id,
  uint16_t session,
  uint16_t seq,
  uint8_t  tx_pow,
  uint8_t  sf,
  uint8_t  cr)
{ // Arrow is a pointer to where the struct lives. Makes this more effecient
  hdr->ver_type = makeVerType(LORA_PROTOCOL_VERSION, type);
  hdr->src_id = src;
  hdr->dst_id = dst;
  hdr->exp_id = exp_id;
  hdr->session_id = session;
  hdr->seq_num = seq;
  hdr->tx_pow = tx_pow;
  hdr->sf_cr = makeSFCR(sf, cr);
}

/*
#   CRC-16
*/

/**
 * @brief Compute CRC-16/CCITT-FALSE over a byte buffer.
 * @param data  Pointer to data
 * @param len   Number of bytes
 * @return      16-bit CRC value
 */
inline uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
  }
  return crc;
}


/*
#   CRC-32
*/

/**
 * @brief Compute CRC-32 over a byte buffer.
 * @param data  Pointer to data
 * @param len   Number of bytes
 * @return      32-bit CRC value
 */
inline uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
      for (uint8_t j = 0; j < 8; j++) {
        crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
      }
  }
  return ~crc; // ~ = NOT op
}


/*
#     Serialization 
*/
/**
 * @param hdr Source header
 * @param buf Destination buffer (must be >= LORA_HEADER_SIZE bytes)
 */
inline void serializeHeader(const LoRaHeader* hdr, uint8_t* buf) {
  memcpy(buf, hdr, LORA_HEADER_SIZE);
}

/**
 * @param buf Source buffer (must be >= LORA_HEADER_SIZE bytes)
 * @param hdr Destination header struct
 */
inline void deserializeHeader(const uint8_t* buf, LoRaHeader* hdr) {
  memcpy(hdr, buf, LORA_HEADER_SIZE);
}


/**
 * @param payload Source payload
 * @param buf Destination buffer (must be >= sizeof(AudioStartPayload))
 */
inline void serializeAudioStart(const AudioStartPayload* payload, uint8_t* buf) {
  memcpy(buf, payload, sizeof(AudioStartPayload));
}

/**
 * @param buf Source buffer (must be >= sizeof(AudioStartPayload))
 * @param payload Desitnation payload
*/
inline void deserializeAudioStart(const uint8_t* buf, AudioStartPayload* payload) {
  memcpy(payload, buf, sizeof(AudioStartPayload));
}



/**
 * @param payload Source payload
 * @param buf Destination buffer (must be >= sizeof(AudioEndPayload))
 */
inline void serializeAudioEnd(const AudioEndPayload* payload, uint8_t* buf) {
  memcpy(buf, payload, sizeof(AudioEndPayload));
}

/**
 * @param buf Source buffer (must be >= sizeof(AudioEndPayload))
 * @param payload Desitnation payload
 */
inline void deserializeAudioEnd(const uint8_t* buf, AudioEndPayload* payload) {
  memcpy(payload, buf, sizeof(AudioEndPayload));
}


/*
#     Print to serial
*/

#ifdef LORA_DEBUG
inline void printHeader(const LoRaHeader* hdr) {
  Serial.println(F("--- LoRa Header ---"));
  Serial.printf("  Version    : %d\n", getVersion(hdr->ver_type));
  Serial.printf("  Type       : 0x%02X\n", getType(hdr->ver_type));
  Serial.printf("  SRC        : 0x%02X\n", hdr->src_id);
  Serial.printf("  DST        : 0x%02X\n", hdr->dst_id);
  Serial.printf("  Experiment : %d\n", hdr->exp_id);
  Serial.printf("  Session    : 0x%04X\n", hdr->session_id);
  Serial.printf("  Seq Num    : %d\n", hdr->seq_num);
  Serial.printf("  TX Power   : %d dBm\n", hdr->tx_pow);
  Serial.printf("  SF         : %d\n", getSF(hdr->sf_cr));
  Serial.printf("  CR         : 4/%d\n", getCR(hdr->sf_cr));
  Serial.println(F("-------------------"));
}

inline void printAudioStart(const AudioStartPayload* p) {
  Serial.println(F("--- Audio Start ---"));
  Serial.printf("  Total Frags: %d\n", p->total_frags);
  Serial.printf("  Codec      : 0x%02X (%s)\n", p->codec_id,
                p->codec_id == CODEC_RAW_PCM ? "Raw PCM" : "Compressed");
  Serial.printf("  Sample Rate: %d Hz\n", p->sample_hz);
  Serial.printf("  Duration   : %d ms\n", p->duration_ms);
  Serial.printf("  Total Size : %lu bytes\n", p->total_size);
  Serial.printf("  CRC16      : 0x%04X\n", p->crc16);
  Serial.println(F("-------------------"));
}
#endif // LORA_DEBUG
