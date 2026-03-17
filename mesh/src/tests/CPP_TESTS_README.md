# C++ Breaking Tests - Quick Start Guide

## What Are These?

**Real breaking tests** that run on your ESP32 hardware and test the **actual C++ implementation**, not simulators. These tests are designed to:

✅ **Find bugs** - Expose missing validation, buffer overflows, null pointer bugs  
✅ **Crash intentionally** - Some tests SHOULD crash to reveal critical bugs  
✅ **Test edge cases** - Zero values, max values, invalid inputs  
✅ **Expose security risks** - Buffer overflow, integer overflow, memory corruption  

## Two Test Sketches

### 1. Unit Tests (`cpp_breaking_tests/`)
Tests packet handling code **WITHOUT hardware requirements**.

```bash
tests/cpp_breaking_tests/cpp_breaking_tests.ino
```

**Tests:**
- Packet serialization/deserialization
- CRC calculations
- Buffer overflow protection
- NULL pointer handling
- Struct size/alignment
- Integer overflow

**Run this first** - doesn't need SD card or LoRa radio.

### 2. Integration Tests (`integration_breaking_tests/`)
Tests LoRaManager and SdManager **WITH actual hardware**.

```bash
tests/integration_breaking_tests/integration_breaking_tests.ino
```

**Tests:**
- SD card operations
- LoRa transmission
- Memory leaks
- Resource exhaustion
- Integration scenarios

**Requires:** SD card inserted, LoRa antenna connected.

## How to Run

### Step 1: Upload Unit Tests First
```
1. Open cpp_breaking_tests/cpp_breaking_tests.ino
2. Upload to ESP32
3. Open Serial Monitor (115200 baud)
4. Watch for crashes or failures
```

### Step 2: Enable Dangerous Tests
In the sketch, find this section:
```cpp
// Uncomment these ONE AT A TIME to find crashes
// test_null_pointer_crc16();
// test_null_pointer_crc32();
// test_massive_length_crc();
// test_buildHeader_null_pointer();
// test_serialize_buffer_too_small();
// test_buffer_overflow_data_payload();
```

**Enable ONE test**, upload, observe if it crashes, then comment it out and try the next.

### Step 3: Run Integration Tests
```
1. Insert SD card
2. Connect LoRa antenna
3. Open integration_breaking_tests/integration_breaking_tests.ino
4. Upload to ESP32
5. Watch for failures that indicate bugs
```

## Expected Results

### ❌ Tests SHOULD Fail When:
- Code accepts invalid inputs without validation
- No bounds checking on arrays
- No null pointer checks
- Missing error handling

### 💥 Tests SHOULD Crash When:
- NULL pointers are dereferenced
- Buffer overflows occur
- Integer overflows in loops
- Stack corruption

### ✅ Tests SHOULD Pass When:
- Code correctly validates inputs
- Bounds checks prevent overflows
- NULL checks prevent crashes
- Error handling is robust

## Reading the Results

### Example 1: Found a Bug ✅
```
[TEST] AudioStart: Zero Sample Rate
  [PASS] Zero sample rate accepted - NO VALIDATION
```
**This is a BUG!** Code should reject zero sample rate.

### Example 2: Missing Validation ⚠️
```
[TEST] AudioData: Length Exceeds Buffer
  [FAIL] Invalid length accepted - BUFFER OVERREAD RISK
```
**Security risk found!** Add validation: `if (len > LORA_MAX_DATA_PAYLOAD) return false;`

### Example 3: Crash Found 💥
```
[TEST] Null Pointer: crc16
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
```
**Critical bug!** Add null check: `if (data == nullptr) return 0xFFFF;`

## What to Fix

Based on tests, you should add:

### 1. Input Validation
```cpp
bool LoRaManager::sendAudioStart(..., uint16_t sample_hz, ...) {
  if (sample_hz == 0) {
    Serial.println("Invalid sample rate");
    return false;
  }
  // ... rest of function
}
```

### 2. Bounds Checking
```cpp
bool LoRaManager::sendAudioData(const uint8_t* data, uint8_t len) {
  if (len > LORA_MAX_DATA_PAYLOAD) {
    Serial.println("Data too large");
    return false;
  }
  // ... rest of function
}
```

### 3. NULL Pointer Checks
```cpp
uint16_t crc16(const uint8_t* data, size_t len) {
  if (data == nullptr) {
    return 0xFFFF;  // Or handle error appropriately
  }
  // ... rest of function
}
```

### 4. Range Validation
```cpp
void buildHeader(..., uint8_t type, ...) {
  if (type > 0x0F) {  // Only 4 bits available
    type = type & 0x0F;  // Or reject
  }
  // ... rest of function
}
```

## Test Categories Summary

| Category | Unit Tests | Integration Tests |
|----------|------------|-------------------|
| Buffer Safety | ✓ | - |
| NULL Pointers | ✓ | - |
| Integer Overflow | ✓ | - |
| CRC Validation | ✓ | - |
| Struct Sizes | ✓ | - |
| SD Operations | - | ✓ |
| LoRa Transmission | - | ✓ |
| Memory Leaks | - | ✓ |
| Integration | - | ✓ |

## Success Metrics

**Good Results:**
- Found 5-10 validation bugs
- Exposed 2-3 crash scenarios
- Identified security risks

**Bad Results:**
- 100% pass rate (tests too weak)
- No crashes (not testing dangerous cases)
- No failures (not exposing bugs)

## Next Steps After Testing

1. **Document all failures** - Create issues for each bug found
2. **Fix critical bugs first** - Crashes, buffer overflows, security risks
3. **Add validation** - Check all inputs before use
4. **Re-test** - Verify fixes work
5. **Add new tests** - For any bugs found in production

---

**Remember:** Finding bugs NOW in testing is MUCH better than finding them in production!
