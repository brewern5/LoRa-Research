#include "LoraManager.h"

bool LoRaManager::init(uint16_t *g_session_id, uint16_t *g_seq_num) {
  int state = _radio.begin(
    TX_FREQ_MHZ,
    TX_BW_KHZ,
    TX_SF,
    TX_CR,
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    TX_POWER_DBM
  );

  _session_id = *g_session_id;
  _seq_num = *g_seq_num;

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LoRa] Init failed, code %d\n", state);
    return false;
  }

  return true;
}

/*
  #     Helpers
*/

/**
 * Send a fully-constructed LoRaAudioPacket over the air.
 * Only transmits header + the portion of the payload that is meaningful
 * for this packet type (avoids sending unused union bytes).
 *
 * @param pkt Pointer to the packet
 * @param payload_len Number of payload bytes to include after the header
 * @return RADIOLIB_ERR_NONE on success
 */
int LoRaManager::_sendPacket(const LoRaAudioPacket* pkt, size_t payload_len) {
  // Total wire bytes = fixed header + actual payload length
  size_t total_len = LORA_HEADER_SIZE + payload_len;

  if (total_len > LORA_MAX_PAYLOAD) {
    Serial.printf("[TX] Packet too large: %u > %u\n", total_len, LORA_MAX_PAYLOAD);
    return -1;
  }

  // Cast struct to raw bytes for RadioLib
  return _radio.transmit(reinterpret_cast<const uint8_t*>(pkt), total_len);
}

void LoRaManager::_fillHeader(LoRaAudioPacket* pkt, uint8_t type) {
  buildHeader(
    &pkt->header,
    type,
    MY_NODE_ID,
    PEER_NODE_ID,
    EXPERIMENT_ID,
    _session_id,
    _seq_num++,
    TX_POWER_DBM,
    TX_SF,
    TX_CR
  );
}

/**
 * Send PKT_AUDIO_START announcing a transfer.
 *
 * @param total_frags  How many data fragments follow
 * @param codec        CODEC_RAW_PCM (0x00) or CODEC_COMPRESSED (0x01)
 * @param sample_hz    Original sample rate in Hz
 * @param duration_ms  Audio duration in milliseconds
 * @param total_size   Total audio data size in bytes
 */
bool LoRaManager::sendAudioStart(uint16_t total_frags, uint8_t codec,
                    uint16_t sample_hz, uint16_t duration_ms,
                    uint32_t total_size) {
  LoRaAudioPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  _fillHeader(&pkt, PKT_AUDIO_START);

  AudioStartPayload& sp = pkt.payload.start;
  sp.total_frags = total_frags;
  sp.codec_id = codec;
  sp.sample_hz = sample_hz;
  sp.duration_ms = duration_ms;
  sp.total_size = total_size;
  // CRC covers everything except the crc16 field itself
  sp.crc16 = crc16(reinterpret_cast<const uint8_t*>(&sp),
                   sizeof(AudioStartPayload) - sizeof(uint16_t));

  int state = _sendPacket(&pkt, sizeof(AudioStartPayload));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[TX] AUDIO_START sent");
  } else {
    Serial.printf("[TX] AUDIO_START failed, code %d\n", state);
  }
  return state == RADIOLIB_ERR_NONE;
}


/**
 * Send a PKT_AUDIO_DATA fragment.
 *
 * @param data  Pointer to raw audio bytes
 * @param len   Number of bytes (must be <= LORA_MAX_DATA_PAYLOAD)
 */
bool LoRaManager::sendAudioData(const uint8_t* data, uint8_t len) {
  if (len > LORA_MAX_DATA_PAYLOAD) {
    Serial.println("[TX] Data chunk too large");
    return false;
  }

  LoRaAudioPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  _fillHeader(&pkt, PKT_AUDIO_DATA);

  AudioDataPayload& dp = pkt.payload.data;
  memcpy(dp.data, data, len);
  dp.len = len;

  // Only send header + len byte + actual data bytes (not the full 245-byte buffer)
  // payload_len = len field (1 byte) + data (len bytes)
  // Because 'len' is the LAST field after data[], we send len bytes of data + 1 byte for len
  uint8_t buf[LORA_MAX_PAYLOAD];
  serializeHeader(&pkt.header, buf);
  memcpy(buf + LORA_HEADER_SIZE, dp.data, len);
  int state = _radio.transmit(buf, LORA_HEADER_SIZE + len);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("[TX] AUDIO_DATA seq=%u len=%u sent\n", _seq_num - 1, len);
  } else {
    Serial.printf("[TX] AUDIO_DATA failed, code %d\n", state);
  }
  return state == RADIOLIB_ERR_NONE;
}


/**
 * Send PKT_AUDIO_END to close the transfer.
 *
 * @param frag_count  Total fragments that were sent
 * @param full_crc32  CRC32 of the complete reassembled audio data
 */
bool LoRaManager::sendAudioEnd(uint16_t frag_count, uint32_t full_crc32) {
  LoRaAudioPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  _fillHeader(&pkt, PKT_AUDIO_END);

  AudioEndPayload& ep = pkt.payload.end;
  ep.frag_count = frag_count;
  ep.crc32      = full_crc32;
  ep.reserved   = 0;

  int state = _sendPacket(&pkt, sizeof(AudioEndPayload));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[TX] AUDIO_END sent");
  } else {
    Serial.printf("[TX] AUDIO_END failed, code %d\n", state);
  }
  return state == RADIOLIB_ERR_NONE;
}


/**
 * Block and wait for an ACK packet. Returns true if ACK_STATUS_OK received
 * for the expected sequence number, false on timeout or error.
 *
 * @param expected_seq  The sequence number we expect to be ACK'd
 * @param timeout_ms    How long to wait in milliseconds
 */
bool LoRaManager::waitForAck(uint16_t expected_seq, uint32_t timeout_ms) {
  uint8_t buf[LORA_MAX_PAYLOAD];
  int state = _radio.receive(buf, sizeof(buf));

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[RX] No ACK received (code %d)\n", state);
    return false;
  }

  // Minimum valid packet = header + AckPayload
  if (_radio.getPacketLength() < LORA_HEADER_SIZE + sizeof(AckPayload)) {
    Serial.println("[RX] ACK packet too short");
    return false;
  }

  LoRaHeader hdr;
  deserializeHeader(buf, &hdr);

  if (getType(hdr.ver_type) != PKT_ACK) {
    Serial.printf("[RX] Expected ACK, got type 0x%02X\n", getType(hdr.ver_type));
    return false;
  }

  AckPayload ack;
  memcpy(&ack, buf + LORA_HEADER_SIZE, sizeof(AckPayload));

  if (ack.ack_seq != expected_seq) {
    Serial.printf("[RX] ACK seq mismatch: got %u, expected %u\n",
                  ack.ack_seq, expected_seq);
    return false;
  }

  if (ack.status != ACK_STATUS_OK) {
    Serial.printf("[RX] ACK error status: 0x%02X\n", ack.status);
    return false;
  }

  Serial.printf("[RX] ACK OK for seq=%u  RSSI=%.1f  SNR=%.1f\n",
                ack.ack_seq, _radio.getRSSI(), _radio.getSNR());
  return true;
}
