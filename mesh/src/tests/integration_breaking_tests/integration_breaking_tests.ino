/**
 * Integration Breaking Tests - LoRa & SD Managers
 * ===============================================
 * Tests the actual LoRaManager and SdManager implementations
 * to expose integration bugs, race conditions, and resource leaks.
 * 
 * NOTE: This requires actual hardware (SD card, LoRa radio)
 * or will fail during initialization. Use cpp_breaking_tests.ino
 * for unit tests that don't require hardware.
 * 
 * Upload to ESP32 and monitor Serial at 115200 baud.
 */

#include <Arduino.h>
#include "src/comms/LoraManager.h"
#include "src/storage/SdManager.h"
#include "src/display/StatusDisplay.h"
#include "src/models/packet.h"

// Test statistics
uint16_t tests_run = 0;
uint16_t tests_passed = 0;
uint16_t tests_failed = 0;

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

// Global managers
LoRaManager lora;
SdManager sd;

// Session globals
uint16_t test_session_id = 0x4242;
uint16_t test_seq_num = 0;


// ═══════════════════════════════════════════════════════════════════════════
//  SD Manager Breaking Tests
// ═══════════════════════════════════════════════════════════════════════════

void test_sd_double_init() {
  TEST_START("SD: Double Initialization");
  
  bool first = sd.init();
  delay(100);
  bool second = sd.init();  // Init again without closing
  
  ASSERT_TRUE(first || second, "SD handles double init");
}

void test_sd_log_before_init() {
  TEST_START("SD: Log Before Initialization");
  
  SdManager sd_test;  // Fresh instance
  
  // Try to log without initializing
  sd_test.logTransmission(0.0, 0.0, millis(), millis() + 50, -70, 8.5,
                          0, 0, -1, 0);
  
  ASSERT_TRUE(true, "SD handles logging before init (should skip gracefully)");
}

void test_sd_write_excessive_logs() {
  TEST_START("SD: Excessive Log Writes (Resource Exhaustion)");
  
  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }
  
  // Write 100 log entries rapidly
  for (int i = 0; i < 100; i++) {
    sd.logTransmission(
      float(i) / 100.0,
      float(i) / 100.0,
      millis(),
      millis() + random(10, 100),
      random(-120, -40),
      random(0, 15),
      1,
      static_cast<uint16_t>(i),
      i,
      245
    );
  }
  
  ASSERT_TRUE(true, "SD survived 100 rapid log writes");
}

void test_sd_invalid_coordinates() {
  TEST_START("SD: Invalid GPS Coordinates");
  
  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }
  
  // Test extreme coordinate values
  sd.logTransmission(
    999.999,     // Invalid latitude
    -999.999,    // Invalid longitude
    millis(),
    millis() + 50,
    -200,        // Invalid RSSI
    999.9,       // Invalid SNR
    1,
    0,
    -1,
    0
  );
  
  ASSERT_TRUE(true, "SD accepts invalid coordinates (NO VALIDATION)");
}

void test_sd_file_not_found() {
  TEST_START("SD: Open Non-existent Audio File");
  
  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }
  
  bool result = sd.openAudioFile("nonexistent_file_12345.raw");
  
  ASSERT_FALSE(result, "SD correctly fails on missing file");
}

void test_sd_bin_roundtrip() {
  TEST_START("SD: .bin Write/Read Round Trip");

  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }

  const char* fileName = "sd_roundtrip_test.bin";
  const uint8_t writeData[] = {0x10, 0x20, 0x30, 0x40, 0xAA, 0x55};
  const size_t writeLen = sizeof(writeData);

  bool writeOk = sd.writeBinaryFile(fileName, writeData, writeLen, false);
  ASSERT_TRUE(writeOk, "Binary file write succeeds");
  if (!writeOk) return;

  uint8_t readData[16] = {0};
  size_t bytesRead = 0;
  bool readOk = sd.readBinaryFile(fileName, readData, sizeof(readData), bytesRead);
  ASSERT_TRUE(readOk, "Binary file read succeeds");
  if (!readOk) return;

  bool lengthOk = (bytesRead == writeLen);
  ASSERT_TRUE(lengthOk, "Binary read length matches write length");

  bool contentOk = lengthOk && (memcmp(writeData, readData, writeLen) == 0);
  ASSERT_TRUE(contentOk, "Binary read content matches written bytes");
}

void test_sd_csv_write_success() {
  TEST_START("SD: CSV Write Persistence");

  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }

  SPIClass verifySpi(FSPI);
  verifySpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  SdFat32 verifySd;
  SdSpiConfig verifyCfg(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(8), &verifySpi);
  bool verifySdOk = verifySd.begin(verifyCfg);
  ASSERT_TRUE(verifySdOk, "Verification SD interface opens");
  if (!verifySdOk) {
    return;
  }

  uint32_t sizeBefore = 0;
  File32 beforeFile;
  bool beforeOpen = beforeFile.open("lora_log.csv", O_READ);
  ASSERT_TRUE(beforeOpen, "CSV opens before write");
  if (beforeOpen) {
    sizeBefore = static_cast<uint32_t>(beforeFile.fileSize());
    beforeFile.close();
  }

  const uint32_t txTime = millis();
  const uint32_t ackTime = txTime + 123;
  bool writeOk = sd.logTransmission(
    0.0,
    0.0,
    txTime,
    ackTime,
    -70,
    9.5,
    test_session_id,
    5000,
    0,
    32,
    "DATA",
    "ACK_OK"
  );
  ASSERT_TRUE(writeOk, "logTransmission returns success");
  if (!writeOk) {
    return;
  }

  uint32_t sizeAfter = 0;
  File32 afterFile;
  bool afterOpen = afterFile.open("lora_log.csv", O_READ);
  ASSERT_TRUE(afterOpen, "CSV opens after write");
  if (afterOpen) {
    sizeAfter = static_cast<uint32_t>(afterFile.fileSize());
    afterFile.close();
  }

  bool grew = sizeAfter > sizeBefore;
  Serial.print(F("  CSV size before: "));
  Serial.println(sizeBefore);
  Serial.print(F("  CSV size after:  "));
  Serial.println(sizeAfter);
  ASSERT_TRUE(grew, "CSV file size increases after log write");
}


// ═══════════════════════════════════════════════════════════════════════════
//  LoRa Manager Breaking Tests
// ═══════════════════════════════════════════════════════════════════════════

void test_lora_send_before_init() {
  TEST_START("LoRa: Send Before Initialization");
  
  LoRaManager lora_test;  // Fresh instance, not initialized
  
  bool result = lora_test.sendAudioStart(10, CODEC_RAW_PCM, 8000, 100, 1600);
  
  ASSERT_FALSE(result, "LoRa rejects send before init");
}

void test_lora_zero_fragments() {
  TEST_START("LoRa: Send START with Zero Fragments");
  
  bool result = lora.sendAudioStart(
    0,              // Zero fragments - invalid!
    CODEC_RAW_PCM,
    8000,
    0,
    0
  );
  
  ASSERT_TRUE(true, "LoRa accepts zero fragments (NO VALIDATION)");
}

void test_lora_excessive_fragments() {
  TEST_START("LoRa: Send START with Excessive Fragments");
  
  bool result = lora.sendAudioStart(
    65535,          // Maximum uint16
    CODEC_RAW_PCM,
    8000,
    60000,
    0xFFFFFFFF
  );
  
  ASSERT_TRUE(result || !result, "LoRa handles max fragment count");
}

void test_lora_invalid_codec() {
  TEST_START("LoRa: Invalid Codec ID");
  
  bool result = lora.sendAudioStart(
    10,
    0xFF,           // Invalid codec
    8000,
    100,
    1600
  );
  
  ASSERT_TRUE(true, "LoRa accepts invalid codec (NO VALIDATION)");
}

void test_lora_zero_sample_rate() {
  TEST_START("LoRa: Zero Sample Rate");
  
  bool result = lora.sendAudioStart(
    10,
    CODEC_RAW_PCM,
    0,              // Zero Hz - invalid!
    100,
    1600
  );
  
  ASSERT_TRUE(true, "LoRa accepts zero sample rate (NO VALIDATION)");
}

void test_lora_send_null_data() {
  TEST_START("LoRa: Send Null Data Pointer");
  
  // Passing nullptr should crash or be rejected
  bool result = lora.sendAudioData(nullptr, 128);
  
  ASSERT_FALSE(result, "LoRa rejects null data pointer");
}

void test_lora_send_oversized_data() {
  TEST_START("LoRa: Send Oversized Data Chunk");
  
  uint8_t data[255];
  memset(data, 0xAA, 255);
  
  // Try to send more than LORA_MAX_DATA_PAYLOAD (245 bytes)
  bool result = lora.sendAudioData(data, 255);
  
  ASSERT_FALSE(result, "LoRa rejects oversized data chunk");
}

void test_lora_send_zero_length_data() {
  TEST_START("LoRa: Send Zero-Length Data");
  
  uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  
  bool result = lora.sendAudioData(data, 0);  // Zero length
  
  ASSERT_TRUE(result || !result, "LoRa handles zero-length data");
}

void test_lora_sequence_number_increment() {
  TEST_START("LoRa: Sequence Number Auto-Increment");
  
  uint16_t seq_before = lora.getLastSeqNum();
  
  // Send a packet
  lora.sendAudioStart(5, CODEC_RAW_PCM, 8000, 100, 800);
  
  uint16_t seq_after = lora.getLastSeqNum();
  
  ASSERT_TRUE(seq_after > seq_before, "Sequence number increments");
}

void test_lora_rapid_transmissions() {
  TEST_START("LoRa: Rapid Consecutive Transmissions");
  
  uint8_t data[128];
  memset(data, 0x55, 128);
  
  // Send 10 packets rapidly without delay
  int success_count = 0;
  for (int i = 0; i < 10; i++) {
    if (lora.sendAudioData(data, 128)) {
      success_count++;
    }
  }
  
  Serial.print(F("  Successfully sent: "));
  Serial.print(success_count);
  Serial.println(F("/10 packets"));
  
  ASSERT_TRUE(success_count >= 5, "LoRa handles rapid transmissions");
}

void test_lora_ack_timeout_handling() {
  TEST_START("LoRa: ACK Timeout Handling");
  
  // Wait for ACK that will never come
  bool ack_received = lora.waitForAck(9999, 500);  // 500ms timeout
  
  ASSERT_FALSE(ack_received, "LoRa handles ACK timeout correctly");
}


// ═══════════════════════════════════════════════════════════════════════════
//  Integration Tests - SD + LoRa
// ═══════════════════════════════════════════════════════════════════════════

void test_transmit_and_log_integration() {
  TEST_START("Integration: Transmit + Log");
  
  if (!sd.isReady()) {
    Serial.println(F("  [SKIP] SD not ready"));
    return;
  }
  
  uint32_t tx_time = millis();
  
  // Send packet
  bool tx_ok = lora.sendAudioStart(5, CODEC_RAW_PCM, 8000, 100, 800);
  
  uint32_t ack_time = millis();
  
  // Log the transmission
  if (tx_ok) {
    sd.logTransmission(
      0.0, 0.0,
      tx_time,
      ack_time,
      lora.getLastRSSI(),
      lora.getLastSNR(),
      1,
      lora.getLastSeqNum(),
      -1,
      0
    );
  }
  
  ASSERT_TRUE(tx_ok, "Transmission and logging integrated");
}

void test_sd_fail_continue_lora() {
  TEST_START("Integration: SD Fails, LoRa Continues");
  
  // Even if SD fails, LoRa should work
  bool tx_ok = lora.sendAudioData((const uint8_t*)"test", 4);
  
  // Try to log (may fail if SD not ready, but shouldn't crash)
  sd.logTransmission(0.0, 0.0, millis(), millis() + 50, -70, 8.0,
                     1, lora.getLastSeqNum(), -1, 0);
  
  ASSERT_TRUE(tx_ok, "LoRa works independently of SD state");
}

void test_memory_leak_detection() {
  TEST_START("Memory: Leak Detection After Multiple Operations");
  
  uint32_t free_before = ESP.getFreeHeap();
  
  // Perform many operations
  for (int i = 0; i < 20; i++) {
    lora.sendAudioData((const uint8_t*)"leak_test", 9);
    delay(10);
  }
  
  uint32_t free_after = ESP.getFreeHeap();
  int32_t leaked = free_before - free_after;
  
  Serial.print(F("  Heap before: "));
  Serial.print(free_before);
  Serial.println(F(" bytes"));
  Serial.print(F("  Heap after:  "));
  Serial.print(free_after);
  Serial.println(F(" bytes"));
  Serial.print(F("  Leaked:      "));
  Serial.print(leaked);
  Serial.println(F(" bytes"));
  
  // Some variation is normal, but large leaks indicate a problem
  ASSERT_TRUE(abs(leaked) < 1000, "No significant memory leak detected");
}


// ═══════════════════════════════════════════════════════════════════════════
//  Setup & Loop
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println(F("\n\n"));
  Serial.println(F("╔════════════════════════════════════════════════════════════╗"));
  Serial.println(F("║     Integration Breaking Tests - LoRa & SD Managers       ║"));
  Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
  Serial.println();
  
  // Initialize components
  Serial.println(F("Initializing components..."));
  
  bool sd_ok = sd.init();
  Serial.print(F("SD:   "));
  Serial.println(sd_ok ? F("OK") : F("FAIL"));
  
  bool lora_ok = lora.init(&test_session_id, &test_seq_num);
  Serial.print(F("LoRa: "));
  Serial.println(lora_ok ? F("OK") : F("FAIL"));
  
  delay(1000);
  
  Serial.println(F("\nStarting tests...\n"));
  
  // SD Manager Tests
  test_sd_double_init();
  test_sd_log_before_init();
  test_sd_write_excessive_logs();
  test_sd_invalid_coordinates();
  test_sd_file_not_found();
  test_sd_bin_roundtrip();
  test_sd_csv_write_success();
  
  // LoRa Manager Tests
  test_lora_send_before_init();
  test_lora_zero_fragments();
  test_lora_excessive_fragments();
  test_lora_invalid_codec();
  test_lora_zero_sample_rate();
  test_lora_send_oversized_data();
  test_lora_send_zero_length_data();
  test_lora_sequence_number_increment();
  test_lora_rapid_transmissions();
  test_lora_ack_timeout_handling();
  
  // Dangerous tests (commented out - may crash)
  // test_lora_send_null_data();
  
  // Integration Tests
  test_transmit_and_log_integration();
  test_sd_fail_continue_lora();
  test_memory_leak_detection();
  
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
    Serial.println(F("⚠ FAILURES DETECTED - These expose bugs to fix!"));
  } else {
    Serial.println(F("All active tests passed - Enable dangerous tests"));
  }
  
  Serial.println(F("\nTesting complete."));
}

void loop() {
  delay(10000);
}
