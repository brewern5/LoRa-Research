#!/usr/bin/env python3
"""
Test Scenario: SD Card OK, LoRa Sending Fails
==============================================
Simulates and validates system behavior when:
- SD card initialization succeeds
- SD card logging works properly
- LoRa radio initialization or transmission fails

This test validates:
1. SD logging continues to work even when LoRa fails
2. Proper error handling when LoRa transmissions fail
3. System resilience and fallback behavior

Usage:
    python sd_ok_lora_fail_test.py
    python sd_ok_lora_fail_test.py --verbose
"""

import argparse
import sys
from enum import Enum


class ComponentStatus(Enum):
    """Component status states"""
    NOT_STARTED = 0
    INITIALIZING = 1
    OK = 2
    FAILED = 3


class TestResult(Enum):
    """Test result states"""
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"


class SDCardSimulator:
    """Simulates SD card functionality"""
    
    def __init__(self, should_work=True):
        self.should_work = should_work
        self.initialized = False
        self.log_file = []
        self.audio_file_open = False
        
    def init(self):
        """Initialize SD card"""
        if self.should_work:
            self.initialized = True
            print("[SD] SPI Started")
            print("[SD] FAT type: 32")
            print("[SD] SD card initialized successfully")
            return True
        else:
            print("[SD] SD init failed - Checking Card")
            return False
    
    def write_log_header(self):
        """Write CSV header to log file"""
        if not self.initialized:
            return False
        header = "nowMs,txTime,ackTime,rttMs,lat,lon,rssi,snr"
        self.log_file.append(header)
        print(f"[SD] Log header written: {header}")
        return True
    
    def log_transmission(self, lat, lon, tx_time, ack_time, rssi, snr):
        """Log a transmission attempt"""
        if not self.initialized:
            print("[SD] Log skipped: SD not ready")
            return False
        
        now_ms = tx_time  # Simplified for testing
        rtt_ms = ack_time - tx_time if ack_time > 0 else -1
        
        log_entry = f"{now_ms},{tx_time},{ack_time},{rtt_ms},{lat},{lon},{rssi},{snr}"
        self.log_file.append(log_entry)
        print(f"[SD] Logged: {log_entry}")
        return True
    
    def open_audio_file(self, filename):
        """Open audio file for reading"""
        if not self.initialized:
            return False
        print(f"[SD] Opened audio file: {filename}")
        self.audio_file_open = True
        return True
    
    def read_audio_chunk(self, chunk_size=128):
        """Read chunk of audio data"""
        if not self.audio_file_open:
            return None
        # Return dummy audio data
        return bytes([0x00] * chunk_size)
    
    def close_audio_file(self):
        """Close audio file"""
        self.audio_file_open = False
        print("[SD] Audio file closed")
    
    def get_log_entries(self):
        """Get all log entries"""
        return self.log_file.copy()


class LoRaSimulator:
    """Simulates LoRa radio functionality"""
    
    def __init__(self, init_should_work=False, send_should_work=False):
        self.init_should_work = init_should_work
        self.send_should_work = send_should_work
        self.initialized = False
        self.packets_sent = []
        self.session_id = 0x1234
        self.seq_num = 0
        
    def init(self):
        """Initialize LoRa radio"""
        if self.init_should_work:
            self.initialized = True
            print("[LoRa] Radio initialized successfully")
            print(f"[LoRa] Session: 0x{self.session_id:04X}")
            return True
        else:
            error_code = -2  # Simulated error code
            print(f"[LoRa] Init failed, code {error_code}")
            return False
    
    def send_audio_start(self, total_frags, codec, sample_hz, duration_ms, total_size):
        """Send AUDIO_START packet"""
        if not self.initialized:
            print("[LoRa] Cannot send: not initialized")
            return False
            
        if self.send_should_work:
            print("[LoRa] AUDIO_START sent")
            self.packets_sent.append(("START", self.seq_num))
            self.seq_num += 1
            return True
        else:
            error_code = -1  # Simulated transmission error
            print(f"[LoRa] AUDIO_START failed, code {error_code}")
            return False
    
    def send_audio_data(self, data, length):
        """Send AUDIO_DATA packet"""
        if not self.initialized:
            print("[LoRa] Cannot send: not initialized")
            return False
            
        if self.send_should_work:
            print(f"[LoRa] AUDIO_DATA seq={self.seq_num} len={length} sent")
            self.packets_sent.append(("DATA", self.seq_num, length))
            self.seq_num += 1
            return True
        else:
            error_code = -1
            print(f"[LoRa] AUDIO_DATA failed, code {error_code}")
            return False
    
    def send_audio_end(self, frag_count, full_crc32):
        """Send AUDIO_END packet"""
        if not self.initialized:
            print("[LoRa] Cannot send: not initialized")
            return False
            
        if self.send_should_work:
            print("[LoRa] AUDIO_END sent")
            self.packets_sent.append(("END", self.seq_num))
            self.seq_num += 1
            return True
        else:
            error_code = -1
            print(f"[LoRa] AUDIO_END failed, code {error_code}")
            return False
    
    def wait_for_ack(self, expected_seq, timeout_ms):
        """Wait for ACK (always fails in this test scenario)"""
        print(f"[LoRa] Waiting for ACK (timeout={timeout_ms}ms)...")
        print("[LoRa] ACK timeout")
        return False
    
    def get_packets_sent(self):
        """Get list of successfully sent packets"""
        return self.packets_sent.copy()


class SystemTest:
    """Test runner for SD OK / LoRa Fail scenario"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.tests_run = 0
        self.tests_passed = 0
        self.tests_failed = 0
        
    def log(self, message):
        """Print verbose log message"""
        if self.verbose:
            print(f"[TEST] {message}")
    
    def assert_true(self, condition, message):
        """Assert that condition is true"""
        self.tests_run += 1
        if condition:
            self.tests_passed += 1
            print(f"  ✓ {message}")
            return TestResult.PASS
        else:
            self.tests_failed += 1
            print(f"  ✗ {message}")
            return TestResult.FAIL
    
    def assert_false(self, condition, message):
        """Assert that condition is false"""
        return self.assert_true(not condition, message)
    
    def test_sd_initialization(self, sd):
        """Test SD card initialization"""
        print("\n[Test 1] SD Card Initialization")
        result = sd.init()
        return self.assert_true(result, "SD card initializes successfully")
    
    def test_sd_logging_when_lora_fails(self, sd, lora):
        """Test that SD logging works when LoRa fails"""
        print("\n[Test 2] SD Logging During LoRa Failure")
        
        # Try to send via LoRa (should fail)
        lora_success = lora.send_audio_start(5, 0x00, 8000, 64, 512)
        self.assert_false(lora_success, "LoRa transmission fails as expected")
        
        # Log the failed transmission attempt to SD
        tx_time = 1000
        ack_time = 0  # No ACK because LoRa failed
        log_success = sd.log_transmission(0.0, 0.0, tx_time, ack_time, 0, 0.0)
        self.assert_true(log_success, "SD logs failed transmission attempt")
        
        # Verify log entry was created
        logs = sd.get_log_entries()
        return self.assert_true(len(logs) >= 2, f"Log file contains entries (found {len(logs)})")
    
    def test_sd_file_operations(self, sd):
        """Test SD card file operations"""
        print("\n[Test 3] SD Card File Operations")
        
        # Open audio file
        open_success = sd.open_audio_file("test_audio.raw")
        self.assert_true(open_success, "SD can open audio file")
        
        # Read chunk
        chunk = sd.read_audio_chunk(128)
        self.assert_true(chunk is not None and len(chunk) == 128, 
                        "SD can read audio chunk")
        
        # Close file
        sd.close_audio_file()
        return self.assert_false(sd.audio_file_open, "SD closes audio file properly")
    
    def test_lora_init_failure(self, lora):
        """Test LoRa initialization failure"""
        print("\n[Test 4] LoRa Initialization Failure")
        result = lora.init()
        return self.assert_false(result, "LoRa initialization fails as expected")
    
    def test_system_continues_with_sd_only(self, sd, lora):
        """Test that system can continue operating with SD only"""
        print("\n[Test 5] System Operation with SD Only")
        
        # Simulate multiple transmission attempts
        for i in range(3):
            self.log(f"Simulating transmission attempt {i+1}")
            
            # LoRa fails
            lora_ok = lora.send_audio_data(bytes([0] * 128), 128)
            self.assert_false(lora_ok, f"LoRa send attempt {i+1} fails")
            
            # But SD logging still works
            sd_ok = sd.log_transmission(0.0, 0.0, 2000 + i*100, 0, 0, 0.0)
            self.assert_true(sd_ok, f"SD logs attempt {i+1}")
        
        # Verify all attempts were logged
        logs = sd.get_log_entries()
        # expected_count = 1 (header) + 1 (from test 2) + 3 (from this test) = 5
        return self.assert_true(len(logs) >= 5, 
                               f"All transmission attempts logged (expected ≥5, got {len(logs)})")
    
    def test_log_content_validation(self, sd):
        """Test that log entries have correct format"""
        print("\n[Test 6] Log Content Validation")
        
        logs = sd.get_log_entries()
        
        # Check header
        if len(logs) > 0:
            header = logs[0]
            expected_fields = ["nowMs", "txTime", "ackTime", "rttMs", "lat", "lon", "rssi", "snr"]
            header_ok = all(field in header for field in expected_fields)
            self.assert_true(header_ok, "Log header contains required fields")
        
        # Check data entries
        if len(logs) > 1:
            data_entry = logs[1]
            field_count = len(data_entry.split(','))
            return self.assert_true(field_count == 8, 
                                   f"Log entries have correct field count (expected 8, got {field_count})")
        
        return TestResult.SKIP
    
    def run_all_tests(self):
        """Run all test cases"""
        print("=" * 60)
        print("Test Scenario: SD Card OK, LoRa Sending Fails")
        print("=" * 60)
        
        # Create component simulators
        sd = SDCardSimulator(should_work=True)
        lora = LoRaSimulator(init_should_work=False, send_should_work=False)
        
        # Run tests
        self.test_sd_initialization(sd)
        sd.write_log_header()
        
        self.test_lora_init_failure(lora)
        self.test_sd_logging_when_lora_fails(sd, lora)
        self.test_sd_file_operations(sd)
        self.test_system_continues_with_sd_only(sd, lora)
        self.test_log_content_validation(sd)
        
        # Print summary
        print("\n" + "=" * 60)
        print("Test Summary")
        print("=" * 60)
        print(f"Tests Run:    {self.tests_run}")
        print(f"Tests Passed: {self.tests_passed}")
        print(f"Tests Failed: {self.tests_failed}")
        print(f"Success Rate: {(self.tests_passed/self.tests_run*100):.1f}%")
        
        return self.tests_failed == 0


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Test scenario where SD works but LoRa fails"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    args = parser.parse_args()
    
    test = SystemTest(verbose=args.verbose)
    success = test.run_all_tests()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
