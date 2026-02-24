#include <RadioLib.h>
#include "packet.h"

// Heltec ESP32 LoRa V2 SX1267 pin mapping
#define LORA_NSS 18
#define LORA_DIO0 26
#define LORA_NRST 14
#define LORA_DI01 33

#define LORA_BUSY 35
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27

// Session config
#define MY_NODE_ID 0x02
#define PEER_NODE_ID 0x01
#define EXPERIMENT_ID 0x01

// LoRa RF parameters
#define RX_FREQ_MHZ 915.0f
#define RX_BW_KHZ 125.0f
#define RX_SF 7
#define RX_CR 5  // 4/5
#define TX_POWER_DBM 14

// At 245 bytes/frag: 512 frags = ~125 KB
#define MAX_FRAGS 22
#define MAX_AUDIO_BYTES (MAX_FRAGS * LORA_MAX_DATA_PAYLOAD)  // ~5.4 KB

static uint8_t g_audio_buf[MAX_AUDIO_BYTES];
static bool g_frag_received[MAX_FRAGS];  // tracks which frags arrived
static uint32_t g_audio_len = 0;         // bytes written so far

// Radio obj
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_NRST, LORA_DI01);

// Session stae
static uint16_t g_session_id = 0;
static uint16_t g_seq_num = 0;


/*
#   Session State
*/
typedef enum {
  STATE_IDLE,
  STATE_RECEIVING,
} RxState;

static RxState g_state = STATE_IDLE;
static uint16_t g_total_frags = 0;
static uint16_t g_frags_rx = 0;
static uint8_t g_codec_id = 0;
static uint16_t g_sample_hz = 0;
static uint16_t g_duration_ms = 0;
static uint32_t g_total_size = 0;



void sendAck(uint16_t ack_seq, uint8_t status) {
  uint8_t buf[LORA_HEADER_SIZE + sizeof(AckPayload)];

  LoRaHeader hdr;
  buildHeader(&hdr, PKT_ACK,
              MY_NODE_ID, PEER_NODE_ID, EXPERIMENT_ID,
              g_session_id, ack_seq,
              TX_POWER_DBM, RX_SF, RX_CR);
  serializeHeader(&hdr, buf);

  AckPayload ack;
  ack.ack_seq = ack_seq;
  ack.status = status;
  memcpy(buf + LORA_HEADER_SIZE, &ack, sizeof(AckPayload));

  int state = radio.transmit(buf, sizeof(buf));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("[ACK] seq=%u status=0x%02X sent\n", ack_seq, status);
  } else {
    Serial.printf("[ACK] send failed, code %d\n", state);
  }
}


void handleAudioStart(const LoRaHeader* hdr, const uint8_t* payload_buf, size_t payload_len) {
  if (payload_len < sizeof(AudioStartPayload)) {
    Serial.println("[RX] AUDIO_START too short");
    return;
  }

  AudioStartPayload sp;
  deserializeAudioStart(payload_buf, &sp);

  // Verify CRC16 (covers struct minus the crc16 field at the end)
  uint16_t computed = crc16(reinterpret_cast<const uint8_t*>(&sp),
                            sizeof(AudioStartPayload) - sizeof(uint16_t));
  if (computed != sp.crc16) {
    Serial.printf("[RX] AUDIO_START CRC16 mismatch: got 0x%04X expected 0x%04X\n",
                  sp.crc16, computed);
    sendAck(hdr->seq_num, ACK_STATUS_CRC_ERR);
    return;
  }

  if (sp.total_frags > MAX_FRAGS) {
    Serial.printf("[RX] total_frags %u exceeds MAX_FRAGS %u — aborting\n",
                  sp.total_frags, MAX_FRAGS);
    sendAck(hdr->seq_num, ACK_STATUS_MISSING);
    return;
  }

  // Reset session
  g_state = STATE_RECEIVING;
  g_session_id = hdr->session_id;
  g_total_frags = sp.total_frags;
  g_frags_rx = 0;
  g_audio_len = 0;
  g_codec_id = sp.codec_id;
  g_sample_hz = sp.sample_hz;
  g_duration_ms = sp.duration_ms;
  g_total_size = sp.total_size;

  memset(g_frag_received, 0, sizeof(g_frag_received));
  memset(g_audio_buf, 0, sizeof(g_audio_buf));

  Serial.println("[RX] ── AUDIO_START ──────────────────");
  Serial.printf("       Session   : 0x%04X\n", g_session_id);
  Serial.printf("       Frags     : %u\n", g_total_frags);
  Serial.printf("       Codec     : 0x%02X (%s)\n", g_codec_id,
                g_codec_id == CODEC_RAW_PCM ? "Raw PCM" : "Compressed");
  Serial.printf("       Sample Hz : %u\n", g_sample_hz);
  Serial.printf("       Duration  : %u ms\n", g_duration_ms);
  Serial.printf("       Total Size: %lu bytes\n", (unsigned long)g_total_size);
  Serial.println("────────────────────────────────────");

  sendAck(hdr->seq_num, ACK_STATUS_OK);
}

void handleAudioData(const LoRaHeader* hdr, const uint8_t* payload_buf, size_t payload_len) {
  if (g_state != STATE_RECEIVING) {
    Serial.println("[RX] DATA received but no active session — ignoring");
    return;
  }

  uint16_t seq = hdr->seq_num;

  // seq_num here is the fragment index within this session
  // We use it to write into the correct offset in the buffer
  // (seq 0 = first data packet after START, so frag index = seq - 1 if START was seq 0,
  //  but transmitter increments g_seq_num for every packet including START/END.
  //  So we track by arrival order using g_frags_rx as the write index.)
  uint16_t frag_index = g_frags_rx;

  if (frag_index >= MAX_FRAGS) {
    Serial.printf("[RX] frag_index %u out of range\n", frag_index);
    return;
  }

  size_t offset = (size_t)frag_index * LORA_MAX_DATA_PAYLOAD;
  if (offset + payload_len > MAX_AUDIO_BYTES) {
    Serial.println("[RX] Buffer overflow — dropping fragment");
    return;
  }

  memcpy(g_audio_buf + offset, payload_buf, payload_len);
  g_frag_received[frag_index] = true;
  g_audio_len += payload_len;
  g_frags_rx++;

  Serial.printf("[RX] DATA frag %u/%u  %u bytes  offset=%u\n",
                g_frags_rx, g_total_frags, payload_len, offset);

  sendAck(seq, ACK_STATUS_OK);
}

void handleAudioEnd(const LoRaHeader* hdr, const uint8_t* payload_buf, size_t payload_len) {
  if (payload_len < sizeof(AudioEndPayload)) {
    Serial.println("[RX] AUDIO_END too short");
    return;
  }

  AudioEndPayload ep;
  deserializeAudioEnd(payload_buf, &ep);

  Serial.println("[RX] ── AUDIO_END ───────────────────");
  Serial.printf("       Frags claimed : %u\n", ep.frag_count);
  Serial.printf("       Frags received: %u\n", g_frags_rx);
  Serial.printf("       Bytes received: %lu\n", (unsigned long)g_audio_len);
  Serial.printf("       CRC32 expected: 0x%08X\n", ep.crc32);

  // Check for missing fragments
  uint16_t missing = 0;
  for (uint16_t i = 0; i < g_total_frags; i++) {
    if (!g_frag_received[i]) missing++;
  }

  if (missing > 0) {
    Serial.printf("       Missing frags : %u\n", missing);
    sendAck(hdr->seq_num, ACK_STATUS_MISSING);
    g_state = STATE_IDLE;
    Serial.println("────────────────────────────────────");
    return;
  }

  // Verify CRC32 over reassembled buffer
  uint32_t computed = crc32(g_audio_buf, g_audio_len);
  Serial.printf("       CRC32 computed: 0x%08X  %s\n",
                computed, computed == ep.crc32 ? "✓ MATCH" : "✗ MISMATCH");

  if (computed != ep.crc32) {
    sendAck(hdr->seq_num, ACK_STATUS_CRC_ERR);
    g_state = STATE_IDLE;
    Serial.println("────────────────────────────────────");
    return;
  }

  sendAck(hdr->seq_num, ACK_STATUS_OK);
  g_state = STATE_IDLE;
  Serial.println("       Transfer complete ✓");
  Serial.println("────────────────────────────────────");

  // ── Hand off reassembled audio here ──────────
  // g_audio_buf contains g_audio_len bytes of audio
  // g_codec_id, g_sample_hz, g_duration_ms are available
  // e.g. writeToSD(g_audio_buf, g_audio_len);
  //      playOverI2S(g_audio_buf, g_audio_len, g_sample_hz);
  onAudioReceived(g_audio_buf, g_audio_len);
}

void onAudioReceived(const uint8_t* buf, uint32_t len) {
  Serial.printf("[AUDIO] %lu bytes ready — codec=0x%02X  %uHz  %ums\n",
                (unsigned long)len, g_codec_id, g_sample_hz, g_duration_ms);

  // Hex dump first 64 bytes as a sanity check
  Serial.print("[AUDIO] First 64 bytes: ");
  for (uint32_t i = 0; i < min(len, (uint32_t)64); i++) {
    Serial.printf("%02X ", buf[i]);
  }
  Serial.println();
}


void receivePacket() {
  uint8_t buf[LORA_MAX_PAYLOAD];
  int state = radio.receive(buf, sizeof(buf), 2000);

  if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    return;  // Nothing received, loop again
  }

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[RX] receive() error, code %d\n", state);
    return;
  }

  size_t pkt_len = radio.getPacketLength();

  if (pkt_len < LORA_HEADER_SIZE) {
    Serial.printf("[RX] Packet too short: %u bytes\n", pkt_len);
    return;
  }

  // Deserialize header
  LoRaHeader hdr;
  deserializeHeader(buf, &hdr);

  // Filter: ignore packets not addressed to us
  if (hdr.dst_id != MY_NODE_ID) {
    Serial.printf("[RX] Ignoring packet for node 0x%02X\n", hdr.dst_id);
    return;
  }

  uint8_t pkt_type = getType(hdr.ver_type);
  uint8_t* payload_buf = buf + LORA_HEADER_SIZE;
  size_t payload_len = pkt_len - LORA_HEADER_SIZE;

  Serial.printf("[RX] type=0x%02X seq=%u src=0x%02X RSSI=%.1f SNR=%.1f\n",
                pkt_type, hdr.seq_num, hdr.src_id,
                radio.getRSSI(), radio.getSNR());

  switch (pkt_type) {
    case PKT_AUDIO_START: handleAudioStart(&hdr, payload_buf, payload_len); break;
    case PKT_AUDIO_DATA: handleAudioData(&hdr, payload_buf, payload_len); break;
    case PKT_AUDIO_END: handleAudioEnd(&hdr, payload_buf, payload_len); break;
    default:
      Serial.printf("[RX] Unknown packet type 0x%02X\n", pkt_type);
      break;
  }
}



void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== LoRa Audio Receiver ===");

 // pinMode(21, OUTPUT);
  //digitalWrite(21, LOW);   // VEXT on
  //delay(500);              // give it more time to stabilize


  int state = radio.begin(RX_FREQ_MHZ);
  Serial.printf("LoRa init state: %d\n", state);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("LoRa init failed, code %d — halting\n", state);
    while (true);
  }

  radio.setSpreadingFactor(RX_SF);
  radio.setBandwidth(RX_BW_KHZ);
  radio.setCodingRate(RX_CR);
  radio.setOutputPower(TX_POWER_DBM);


  Serial.println("Listening...\n");
}


void loop() {
  receivePacket();
}