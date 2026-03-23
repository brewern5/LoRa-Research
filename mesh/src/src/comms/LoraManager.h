#pragma once
#include <RadioLib.h>
#include "../storage/SdManager.h"
#include "../models/packet.h"
#include "../../mesh_role_config.h"

// Heltec ESP32 LoRa V3 SX1262 pin mapping
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_NRST 12
#define LORA_BUSY 13

// LoRa RF parameters (defaults can be overridden in mesh_role_config.h)
#ifndef MESH_LORA_FREQ_MHZ
#define MESH_LORA_FREQ_MHZ 915.0f
#endif

#ifndef MESH_LORA_BW_KHZ
#define MESH_LORA_BW_KHZ 125.0f
#endif

#ifndef MESH_LORA_SF
#define MESH_LORA_SF 7
#endif

#ifndef MESH_LORA_CR
#define MESH_LORA_CR 5
#endif

#ifndef MESH_LORA_TX_POWER_DBM
#define MESH_LORA_TX_POWER_DBM 14
#endif

#ifndef MESH_NODE_ID
#define MESH_NODE_ID 0x01
#endif

#ifndef MESH_PEER_NODE_ID
#define MESH_PEER_NODE_ID 0x02
#endif

#ifndef MESH_EXPERIMENT_ID
#define MESH_EXPERIMENT_ID 0x01
#endif

class LoRaManager {
    public:
        bool init(uint16_t *g_session_id, uint16_t *g_seq_num);
        void resetSPI();

        bool sendAudioStart(uint16_t total_frags, uint8_t codec,
                    uint16_t sample_hz, uint16_t duration_ms,
                    uint32_t total_size);
        bool sendAudioData(const uint8_t* data, uint8_t len);
        bool sendAudioEnd(uint16_t frag_count, uint32_t full_crc32);
        bool waitForAck(uint16_t expected_seq, uint32_t timeout_ms);
        bool receiveRaw(uint8_t* out, size_t out_size, size_t* received_len = nullptr);
        bool sendAckFor(const LoRaHeader& receivedHeader, uint8_t status = ACK_STATUS_OK);

        // Expose for logging after ACK
        float getLastRSSI() { return _radio.getRSSI(); }
        float getLastSNR() { return _radio.getSNR(); }
        uint16_t getLastSeqNum() { return _seq_num - 1; } // Return the last sent seq num
    private:
      // Radio obj
      SX1262 _radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

      uint16_t _session_id;
      uint16_t _seq_num;

      int  _sendPacket(const LoRaAudioPacket* pkt, size_t payload_len);
      void _fillHeader(LoRaAudioPacket* pkt, uint8_t type);
};