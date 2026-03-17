# C++ Breaking Tests

## Overview

These are **breaking tests** that run on actual ESP32 hardware to expose bugs in the real C++ implementation. Unlike the Python simulation tests, these tests interact with your actual code and can:

- **Crash the ESP32** (revealing critical bugs)
- **Expose buffer overflows** (security vulnerabilities)
- **Find memory corruption** (data integrity issues)
- **Detect missing validation** (edge case handling)

## Expected Results

**A 100% pass rate is SUSPICIOUS and means tests aren't aggressive enough.**

Real breaking tests should:
- ❌ **Fail** when they expose missing validation
- 💥 **Crash** when they trigger buffer overflows or null pointer dereferences
- ⚠️ **Pass with warnings** when edge cases are handled (but note the risk)

## How to Run

### Step 1: Upload to ESP32

1. Open `cpp_breaking_tests.ino` in Arduino IDE
2. Select your ESP32 board (Heltec WiFi LoRa 32 V3)
3. Upload the sketch
4. Open Serial Monitor at **115200 baud**

### Step 2: Observe Results

Watch for:
- `[PASS]` - Test passed (code handled edge case)
- `[FAIL]` - **Test exposed a bug** (missing validation, wrong behavior)
- ESP32 reboot - **Test crashed the system** (critical bug found)

### Step 3: Enable Dangerous Tests (One at a Time!)

The sketch has a "DANGER ZONE" with tests commented out because **they will likely crash**:

```cpp
// Uncomment ONE AT A TIME to find crashes
// test_null_pointer_crc16();           // NULL pointer dereference
// test_null_pointer_crc32();           // NULL pointer dereference
// test_massive_length_crc();           // Integer overflow loop
// test_buildHeader_null_pointer();     // NULL struct pointer
// test_serialize_buffer_too_small();   // Buffer overflow
// test_buffer_overflow_data_payload(); // Heap corruption
```

**Testing procedure:**
1. Uncomment the first dangerous test
2. Upload and observe (does it crash?)
3. Comment it back out
4. Repeat for next test

## Test Categories

### Memory Safety Tests
- `test_buffer_overflow_data_payload()` - Write past array bounds
- `test_serialize_buffer_too_small()` - Serialize into small buffer
- `test_null_pointer_crc16/32()` - NULL pointer handling
- `test_buildHeader_null_pointer()` - NULL struct handling

### Overflow Tests
- `test_makeVerType_overflow()` - Nibble masking
- `test_sequence_number_overflow()` - uint16 wrap
- `test_massive_length_crc()` - Integer overflow in loops

### Validation Tests
- `test_data_payload_length_overflow()` - Length field validation
- `test_codec_invalid_value()` - Enum validation
- `test_sample_rate_zero()` - Zero value handling
- `test_fragment_count_zero()` - Minimum bound check
- `test_fragment_count_max()` - Maximum bound check

### Data Integrity Tests
- `test_audio_start_crc_tamper_detection()` - CRC detects changes
- `test_end_payload_frag_count_mismatch()` - Consistency checking
- `test_deserialize_corrupted_data()` - Corrupted input handling

### Structure Tests
- `test_header_struct_size()` - Packing validation
- `test_memory_alignment()` - Alignment verification
- `test_union_size_consistency()` - Union member sizes

## Example Bugs to Find

### Buffer Overflow
```cpp
AudioDataPayload payload;
payload.len = 255;  // Says 255 bytes
// But data[] is only 245 bytes!
// Receiver will read 10 bytes of garbage
```

### NULL Pointer
```cpp
uint16_t crc = crc16(nullptr, 100);
// No null check = crash!
```

### Missing Validation
```cpp
AudioStartPayload p;
p.total_frags = 0;     // Zero fragments?
p.codec_id = 0xFF;     // Invalid codec?
p.sample_hz = 0;       // Zero sample rate?
// All accepted without validation!
```

## Interpreting Results

### Scenario 1: Test Passes
```
[PASS] Zero sample rate accepted - NO VALIDATION
```
⚠️ **This is actually BAD** - code accepts invalid input!

### Scenario 2: Test Fails
```
[FAIL] Invalid length accepted - BUFFER OVERREAD RISK
```
✅ **This is GOOD** - test found a bug!

### Scenario 3: ESP32 Crashes/Reboots
```
[TEST] Null Pointer: crc16
Guru Meditation Error: Core  0 panic'ed (LoadProhibited)
```
💥 **Critical bug found** - no null pointer check!

## What to Do with Failures

1. **Document the bug** in your issue tracker
2. **Add validation** to prevent invalid inputs
3. **Add null checks** before dereferencing pointers
4. **Add bounds checks** before array access
5. **Re-run test** to verify fix

## Safety Notes

- Tests run in `setup()` only once
- Some tests **intentionally** try to crash the system
- Enable dangerous tests **one at a time**
- Keep Serial Monitor open to see crash messages
- ESP32 will auto-reboot after crash

## Success Criteria

**Good test suite results:**
- Found 5-10 bugs (missing validation, no null checks)
- Crashed 2-3 times (exposed critical bugs)
- Identified security risks (buffer overflows)

**Bad test suite results:**
- 100% pass rate (tests aren't aggressive enough)
- No crashes (not testing dangerous edge cases)
- No failures (not exposing bugs)

Remember: **The goal is to FIND and FIX bugs NOW, not in production!**
