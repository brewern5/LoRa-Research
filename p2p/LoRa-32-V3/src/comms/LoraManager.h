#pragma once
#include <RadioLib.h>
#include "src/storage/SdManager.h"
#include "src/models/packet.h"

// Heltec ESP32 LoRa V3 SX1262 pin mapping
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_NRST 12
#define LORA_BUSY 13

// LoRa RF parameters
#define TX_FREQ_MHZ 915.0f
#define TX_BW_KHZ 125.0f
#define TX_SF 7
#define TX_CR 5 // 4/5
#define TX_POWER_DBM 14

// Session config
#define MY_NODE_ID 0x01
#define PEER_NODE_ID 0x02
#define EXPERIMENT_ID 0x01

class LoRaManager {
    public:
        bool init(uint16_t *g_session_id, uint16_t *g_seq_num);

        bool sendAudioStart(uint16_t total_frags, uint8_t codec,
                    uint16_t sample_hz, uint16_t duration_ms,
                    uint32_t total_size);
        bool sendAudioData(const uint8_t* data, uint8_t len);
        bool sendAudioEnd(uint16_t frag_count, uint32_t full_crc32);
        bool waitForAck(uint16_t expected_seq, uint32_t timeout_ms = 2000);
        
        // Helpers
        int sendPacket(const LoRaAudioPacket* pkt, size_t payload_len);
        void fillHeader(LoRaAudioPacket* pkt, uint8_t type);
        bool sendAudioStart(uint16_t total_frags, uint8_t codec,
                    uint16_t sample_hz, uint16_t duration_ms,
                    uint32_t total_size);
        bool sendAudioData(const uint8_t* data, uint8_t len);
        bool sendAudioEnd(uint16_t frag_count, uint32_t full_crc32);
        bool waitForAck(uint16_t expected_seq, uint32_t timeout_ms = 2000);

        // Expose for logging after ACK
        float getLastRSSI() { return _radio.getRSSI(); }
        float getLastSNR() { return _radio.getSNR(); }
    private:
      // Radio obj
      SX1262 _radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

      uint16_t _session_id;
      uint16_t _seq_num;

      int  _sendPacket(const LoRaAudioPacket* pkt, size_t payload_len);
      void _fillHeader(LoRaAudioPacket* pkt, uint8_t type);
};