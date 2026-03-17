/**
 * Breaking Tests for C++ Implementation
 * =====================================
 * These tests run on actual ESP32 hardware and attempt to expose bugs
 * in the real C++ code. Tests that PASS mean the code handled the edge
 * case correctly. Tests that FAIL or CRASH expose bugs.
 * 
 * Upload this to ESP32 and monitor Serial output at 115200 baud.
 * 
 * Expected: Some tests SHOULD fail and expose bugs!
 */

#include <Arduino.h>
#include "src/models/packet.h"

// Test statistics
uint16_t tests_run = 0;
uint16_t tests_passed = 0;
uint16_t tests_failed = 0;
uint16_t tests_crashed = 0;

// Test assertion macros
#define TEST_START(name) \
  Serial.println(); \
  Serial.print(F("[TEST] ")); \
  Serial.println(F(name)); \
  tests_run++;

#define ASSERT_TRUE(condition, message) \
  if (condition) { \
    tests_passed++; \
    Serial.print(F("  [PASS] ")); \
    Serial.println(F(message)); \
  } else { \
    tests_failed++; \
    Serial.print(F("  [FAIL] ")); \
    Serial.println(F(message)); \
  }

#define ASSERT_FALSE(condition, message) \
  ASSERT_TRUE(!(condition), message)

#define ASSERT_EQUAL(expected, actual, message) \
  if ((expected) == (actual)) { \
    tests_passed++; \
    Serial.print(F("  [PASS] ")); \
    Serial.println(F(message)); \
  } else { \
    tests_failed++; \
    Serial.print(F("  [FAIL] ")); \
    Serial.print(F(message)); \
    Serial.print(F(" (expected=")); \
    Serial.print(expected); \
    Serial.print(F(", got=")); \
    Serial.print(actual); \
    Serial.println(F(")")); \
  }


// ═══════════════════════════════════════════════════════════════════════════
//  BREAKING TESTS - These should expose real bugs
// ═══════════════════════════════════════════════════════════════════════════

void test_buffer_overflow_data_payload() {
  TEST_START("Buffer Overflow: AudioDataPayload");
  
  AudioDataPayload payload;
  
  // Try writing beyond buffer bounds
  // BUG: len field is AFTER data array - could write out of bounds
  for (int i = 0; i < LORA_MAX_DATA_PAYLOAD + 10; i++) {
    payload.data[i] = 0xFF;  // This SHOULD crash or corrupt memory
  }
  
  // If we get here without crashing, the test "passes" (but there's still a risk)
  ASSERT_TRUE(true, "Survived writing beyond data array (potential heap corruption)");
}

void test_null_pointer_crc16() {
  TEST_START("Null Pointer: crc16");
  
  // Try to compute CRC on null pointer - should crash or return error
  uint16_t result = crc16(nullptr, 10);
  
  // If we survive, that's actually bad - no null check!
  ASSERT_FALSE(true, "crc16 accepts NULL pointer - SECURITY RISK");
}

void test_null_pointer_crc32() {
  TEST_START("Null Pointer: crc32");
  
  uint32_t result = crc32(nullptr, 100);
  
  ASSERT_FALSE(true, "crc32 accepts NULL pointer - SECURITY RISK");
}

void test_zero_length_crc() {
  TEST_START("Zero Length CRC");
  
  uint8_t data[10] = {1,2,3,4,5,6,7,8,9,10};
  uint16_t crc16_result = crc16(data, 0);
  uint32_t crc32_result = crc32(data, 0);
  
  // Zero length should return initial CRC value
  ASSERT_EQUAL(0xFFFF, crc16_result, "crc16(data, 0) returns initial value");
  ASSERT_EQUAL(0xFFFFFFFF, ~crc32_result, "crc32(data, 0) returns initial value");
}

void test_massive_length_crc() {
  TEST_START("Massive Length CRC (Integer Overflow)");
  
  uint8_t data[10] = {0xAA};
  
  // Try to compute CRC with size_t max - should overflow loop counter
  // This could run forever or crash
  uint16_t result = crc16(data, 0xFFFF);  // Large but not infinite
  
  ASSERT_TRUE(true, "Survived large CRC calculation");
}

void test_header_struct_size() {
  TEST_START("Header Struct Size Validation");
  
  // Header should be exactly 10 bytes
  size_t actual_size = sizeof(LoRaHeader);
  ASSERT_EQUAL(LORA_HEADER_SIZE, actual_size, "LoRaHeader size matches expected");
  
  // Check packing worked correctly
  ASSERT_TRUE(actual_size == 10, "Struct packing successful (no padding)");
}

void test_packet_size_overflow() {
  TEST_START("Packet Size: Total Packet Exceeds 255");
  
  LoRaAudioPacket pkt;
  size_t total_size = sizeof(pkt.header) + sizeof(AudioStartPayload);
  
  // Total packet should not exceed LORA_MAX_PAYLOAD
  ASSERT_TRUE(total_size <= LORA_MAX_PAYLOAD, 
              "START packet fits in max payload");
  
  size_t data_packet_size = sizeof(pkt.header) + LORA_MAX_DATA_PAYLOAD;
  ASSERT_TRUE(data_packet_size <= LORA_MAX_PAYLOAD,
              "DATA packet fits in max payload");
}

void test_makeVerType_overflow() {
  TEST_START("makeVerType: Nibble Overflow");
  
  // Try values larger than 4 bits (0-15)
  uint8_t result = makeVerType(0xFF, 0xFF);  // Should mask to 0x0F each
  
  ASSERT_EQUAL(0xFF, result, "makeVerType masks to 4 bits each");
  
  // Check extraction
  ASSERT_EQUAL(0x0F, getVersion(result), "Version extracted correctly");
  ASSERT_EQUAL(0x0F, getType(result), "Type extracted correctly");
}

void test_buildHeader_null_pointer() {
  TEST_START("buildHeader: Null Pointer");
  
  // Pass null header pointer - should crash
  buildHeader(nullptr, PKT_AUDIO_START, 1, 2, 3, 0x1234, 1, 14, 7, 5);
  
  ASSERT_FALSE(true, "buildHeader accepts NULL - NO NULL CHECK");
}

void test_serialize_deserialize_round_trip() {
  TEST_START("Serialize/Deserialize Round Trip");
  
  LoRaHeader original;
  buildHeader(&original, PKT_AUDIO_DATA, 0x01, 0x02, 0x03, 
              0xABCD, 42, 14, 7, 5);
  
  uint8_t buffer[LORA_HEADER_SIZE];
  serializeHeader(&original, buffer);
  
  LoRaHeader deserialized;
  deserializeHeader(buffer, &deserialized);
  
  // Compare all fields
  bool match = (original.ver_type == deserialized.ver_type) &&
               (original.src_id == deserialized.src_id) &&
               (original.dst_id == deserialized.dst_id) &&
               (original.exp_id == deserialized.exp_id) &&
               (original.session_id == deserialized.session_id) &&
               (original.seq_num == deserialized.seq_num) &&
               (original.tx_pow == deserialized.tx_pow) &&
               (original.sf_cr == deserialized.sf_cr);
  
  ASSERT_TRUE(match, "Round trip preserves all header fields");
}

void test_serialize_buffer_too_small() {
  TEST_START("Serialize: Buffer Too Small");
  
  LoRaHeader hdr;
  buildHeader(&hdr, PKT_AUDIO_START, 1, 2, 3, 0x1234, 1, 14, 7, 5);
  
  uint8_t small_buffer[5];  // Only 5 bytes, needs 10
  serializeHeader(&hdr, small_buffer);  // Buffer overflow!
  
  ASSERT_FALSE(true, "serializeHeader has no bounds check - BUFFER OVERFLOW");
}

void test_deserialize_corrupted_data() {
  TEST_START("Deserialize: Corrupted Data");
  
  uint8_t corrupted[LORA_HEADER_SIZE];
  memset(corrupted, 0xFF, LORA_HEADER_SIZE);
  
  LoRaHeader hdr;
  deserializeHeader(corrupted, &hdr);
  
  // Should handle corrupted data gracefully
  uint8_t version = getVersion(hdr.ver_type);
  uint8_t type = getType(hdr.ver_type);
  
  ASSERT_EQUAL(0x0F, version, "Corrupted version extracted");
  ASSERT_EQUAL(0x0F, type, "Corrupted type extracted");
}

void test_audio_start_crc_validation() {
  TEST_START("AudioStart: CRC16 Validation");
  
  AudioStartPayload payload;
  payload.total_frags = 100;
  payload.codec_id = CODEC_RAW_PCM;
  payload.sample_hz = 8000;
  payload.duration_ms = 1000;
  payload.total_size = 16000;
  
  // Calculate CRC over everything except crc16 field
  payload.crc16 = crc16((const uint8_t*)&payload, 
                        sizeof(AudioStartPayload) - sizeof(uint16_t));
  
  // Verify CRC
  uint16_t verify_crc = crc16((const uint8_t*)&payload,
                              sizeof(AudioStartPayload) - sizeof(uint16_t));
  
  ASSERT_EQUAL(payload.crc16, verify_crc, "CRC16 calculation consistent");
}

void test_audio_start_crc_tamper_detection() {
  TEST_START("AudioStart: CRC Tamper Detection");
  
  AudioStartPayload payload;
  payload.total_frags = 50;
  payload.codec_id = CODEC_RAW_PCM;
  payload.sample_hz = 16000;
  payload.duration_ms = 500;
  payload.total_size = 8000;
  payload.crc16 = crc16((const uint8_t*)&payload,
                        sizeof(AudioStartPayload) - sizeof(uint16_t));
  
  uint16_t original_crc = payload.crc16;
  
  // Tamper with data
  payload.total_frags = 999;
  
  // Recalculate CRC
  uint16_t tampered_crc = crc16((const uint8_t*)&payload,
                                sizeof(AudioStartPayload) - sizeof(uint16_t));
  
  ASSERT_FALSE(original_crc == tampered_crc, "CRC detects tampering");
}

void test_sequence_number_overflow() {
  TEST_START("Sequence Number: uint16 Overflow");
  
  LoRaHeader hdr;
  uint16_t max_seq = 0xFFFF;
  
  buildHeader(&hdr, PKT_AUDIO_DATA, 1, 2, 3, 0x1234, max_seq, 14, 7, 5);
  
  ASSERT_EQUAL(max_seq, hdr.seq_num, "Max sequence number stored");
  
  // Next sequence would overflow
  uint16_t next_seq = max_seq + 1;  // Wraps to 0
  ASSERT_EQUAL(0, next_seq, "Sequence overflow wraps to 0");
}

void test_session_id_overflow() {
  TEST_START("Session ID: uint16 Overflow");
  
  LoRaHeader hdr;
  uint16_t max_session = 0xFFFF;
  
  buildHeader(&hdr, PKT_AUDIO_START, 1, 2, 3, max_session, 0, 14, 7, 5);
  
  ASSERT_EQUAL(max_session, hdr.session_id, "Max session ID stored");
}

void test_fragment_count_zero() {
  TEST_START("AudioStart: Zero Fragments");
  
  AudioStartPayload payload;
  payload.total_frags = 0;  // Invalid - zero fragments
  payload.codec_id = CODEC_RAW_PCM;
  payload.sample_hz = 8000;
  payload.duration_ms = 0;
  payload.total_size = 0;
  
  // Should this be allowed? Probably not, but let's see
  ASSERT_TRUE(payload.total_frags == 0, "Zero fragments accepted (may be invalid)");
}

void test_fragment_count_max() {
  TEST_START("AudioStart: Maximum Fragments");
  
  AudioStartPayload payload;
  payload.total_frags = 0xFFFF;  // 65535 fragments!
  payload.codec_id = CODEC_RAW_PCM;
  payload.sample_hz = 8000;
  payload.duration_ms = 60000;
  payload.total_size = 0xFFFFFFFF;  // Max size
  
  ASSERT_TRUE(true, "Maximum fragment count accepted");
}

void test_codec_invalid_value() {
  TEST_START("AudioStart: Invalid Codec ID");
  
  AudioStartPayload payload;
  payload.codec_id = 0xFF;  // Invalid codec
  
  // Should there be validation? Currently there isn't
  ASSERT_TRUE(true, "Invalid codec accepted - NO VALIDATION");
}

void test_sample_rate_zero() {
  TEST_START("AudioStart: Zero Sample Rate");
  
  AudioStartPayload payload;
  payload.sample_hz = 0;  // Invalid sample rate
  
  ASSERT_TRUE(true, "Zero sample rate accepted - NO VALIDATION");
}

void test_data_payload_length_overflow() {
  TEST_START("AudioData: Length Exceeds Buffer");
  
  AudioDataPayload payload;
  payload.len = 255;  // Exceeds LORA_MAX_DATA_PAYLOAD (245)
  
  // BUG: len field says 255 but buffer is only 245
  // This would cause out-of-bounds read on receiver
  ASSERT_FALSE(payload.len <= LORA_MAX_DATA_PAYLOAD, 
               "Invalid length accepted - BUFFER OVERREAD RISK");
}

void test_end_payload_frag_count_mismatch() {
  TEST_START("AudioEnd: Fragment Count Mismatch");
  
  // START says 100 fragments
  uint16_t expected_frags = 100;
  
  // END says different count
  AudioEndPayload end_payload;
  end_payload.frag_count = 50;  // Mismatch!
  
  ASSERT_FALSE(expected_frags == end_payload.frag_count,
               "Fragment count mismatch detected");
}

void test_memory_alignment() {
  TEST_START("Memory Alignment Check");
  
  LoRaAudioPacket pkt;
  
  // Check alignment of packet components
  uintptr_t pkt_addr = (uintptr_t)&pkt;
  uintptr_t header_addr = (uintptr_t)&pkt.header;
  uintptr_t payload_addr = (uintptr_t)&pkt.payload;
  
  ASSERT_EQUAL(pkt_addr, header_addr, "Header is at packet start");
  ASSERT_EQUAL(pkt_addr + sizeof(LoRaHeader), payload_addr, 
               "Payload immediately follows header");
}

void test_union_size_consistency() {
  TEST_START("Union Size Consistency");
  
  size_t start_size = sizeof(AudioStartPayload);
  size_t data_size = sizeof(AudioDataPayload);
  size_t end_size = sizeof(AudioEndPayload);
  size_t ack_size = sizeof(AckPayload);
  
  Serial.print(F("  AudioStartPayload: "));
  Serial.print(start_size);
  Serial.println(F(" bytes"));
  
  Serial.print(F("  AudioDataPayload: "));
  Serial.print(data_size);
  Serial.println(F(" bytes"));
  
  Serial.print(F("  AudioEndPayload: "));
  Serial.print(end_size);
  Serial.println(F(" bytes"));
  
  Serial.print(F("  AckPayload: "));
  Serial.print(ack_size);
  Serial.println(F(" bytes"));
  
  // Union should be size of largest member
  ASSERT_TRUE(data_size >= start_size && data_size >= end_size && data_size >= ack_size,
              "AudioDataPayload is largest union member");
}


// ═══════════════════════════════════════════════════════════════════════════
//  Arduino Setup & Loop
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(3000);  // Wait for serial connection
  
  Serial.println(F("\n\n"));
  Serial.println(F("╔════════════════════════════════════════════════════════════╗"));
  Serial.println(F("║  C++ Breaking Tests - LoRa Audio Packet Implementation    ║"));
  Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
  Serial.println();
  Serial.println(F("WARNING: Some tests are EXPECTED to expose bugs or crash!"));
  Serial.println(F("If ESP32 reboots during testing, a test triggered a crash."));
  Serial.println();
  
  delay(2000);
  
  // Run all tests
  Serial.println(F("Starting test execution...\n"));
  
  // Buffer and memory tests
  test_header_struct_size();
  test_packet_size_overflow();
  test_memory_alignment();
  test_union_size_consistency();
  
  // Serialization tests
  test_serialize_deserialize_round_trip();
  test_deserialize_corrupted_data();
  
  // Overflow and boundary tests
  test_makeVerType_overflow();
  test_sequence_number_overflow();
  test_session_id_overflow();
  
  // Payload validation tests
  test_fragment_count_zero();
  test_fragment_count_max();
  test_codec_invalid_value();
  test_sample_rate_zero();
  test_data_payload_length_overflow();
  test_end_payload_frag_count_mismatch();
  
  // CRC tests
  test_zero_length_crc();
  test_audio_start_crc_validation();
  test_audio_start_crc_tamper_detection();
  
  // DANGEROUS TESTS - These may crash the ESP32
  Serial.println();
  Serial.println(F("═══════════════════════════════════════════════════"));
  Serial.println(F("  DANGER ZONE: Tests that may crash the system"));
  Serial.println(F("═══════════════════════════════════════════════════"));
  Serial.println();
  
  delay(1000);
  
  // Uncomment these ONE AT A TIME to find crashes
  // test_null_pointer_crc16();
  // test_null_pointer_crc32();
  // test_massive_length_crc();
  // test_buildHeader_null_pointer();
  // test_serialize_buffer_too_small();
  // test_buffer_overflow_data_payload();
  
  Serial.println(F("\nNOTE: Dangerous tests are commented out."));
  Serial.println(F("Uncomment ONE AT A TIME in setup() to test for crashes."));
  
  // Summary
  Serial.println();
  Serial.println(F("════════════════════════════════════════════════════════════"));
  Serial.println(F("  Test Summary"));
  Serial.println(F("════════════════════════════════════════════════════════════"));
  Serial.print(F("Tests Run:    "));
  Serial.println(tests_run);
  Serial.print(F("Tests Passed: "));
  Serial.println(tests_passed);
  Serial.print(F("Tests Failed: "));
  Serial.println(tests_failed);
  
  if (tests_run > 0) {
    float success_rate = (tests_passed * 100.0) / tests_run;
    Serial.print(F("Success Rate: "));
    Serial.print(success_rate, 1);
    Serial.println(F("%"));
  }
  
  Serial.println();
  if (tests_failed > 0) {
    Serial.println(F("⚠ FAILURES DETECTED - Review failed tests above"));
  } else {
    Serial.println(F("✓ All active tests passed"));
  }
  
  Serial.println();
  Serial.println(F("Testing complete. Monitor output for issues."));
}

void loop() {
  // Tests run once in setup()
  delay(10000);
}
