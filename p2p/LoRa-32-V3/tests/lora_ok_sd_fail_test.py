#!/usr/bin/env python3
"""
Test Scenario: LoRa OK, SD Card Fails
======================================
Simulates and validates system behavior when:
- LoRa radio initialization succeeds
- LoRa transmissions work properly
- SD card initialization or operations fail

This test validates:
1. LoRa operations continue when SD fails
2. System degrades gracefully (no logging but transmission works)
3. No crashes or hangs when logging fails
4. Transmission data remains intact despite SD failure

Usage:
    python lora_ok_sd_fail_test.py
    python lora_ok_sd_fail_test.py --verbose
"""

import argparse
import sys
from enum import Enum


class TestResult(Enum):
    """Test result states"""
    PASS = "PASS"
    FAIL = "FAIL"


class SDCardSimulator:
    """Simulates SD card with failure modes"""
    
    def __init__(self, init_fails=True, write_fails=False):
        self.init_fails = init_fails
        self.write_fails = write_fails
        self.initialized = False
        self.log_file = []
        self.init_attempts = 0
        self.write_attempts = 0
        
    def init(self):
        """Initialize SD card (fails in this scenario)"""
        self.init_attempts += 1
        if self.init_fails:
            print("[SD] SD init failed - Checking Card")
            print("[SD] SD Error Code - 0x03")
            return False
        else:
            self.initialized = True
            print("[SD] SD card initialized successfully")
            return True
    
    def write_log_header(self):
        """Write CSV header (fails if not initialized)"""
        if not self.initialized:
            print("[SD] Log header write failed: SD not initialized")
            return False
        print("[SD] Log header written")
        return True
    
    def log_transmission(self, lat, lon, tx_time, ack_time, rssi, snr):
        """Log transmission (fails if SD not ready)"""
        self.write_attempts += 1
        if not self.initialized:
            print("[SD] Log skipped: SD not ready")
            return False
        if self.write_fails:
            print("[SD] Log write failed: I/O error")
            return False
        
        log_entry = f"{tx_time},{tx_time},{ack_time},{ack_time-tx_time},{lat},{lon},{rssi},{snr}"
        self.log_file.append(log_entry)
        print(f"[SD] Logged: {log_entry}")
        return True


class LoRaSimulator:
    """Simulates working LoRa radio"""
    
    def __init__(self):
        self.initialized = False
        self.packets_sent = []
        self.session_id = 0x1234
        self.seq_num = 0
        self.total_bytes_sent = 0
        
    def init(self):
        """Initialize LoRa radio (succeeds)"""
        self.initialized = True
        print("[LoRa] Radio initialized successfully")
        print(f"[LoRa] Session: 0x{self.session_id:04X}")
        return True
    
    def send_audio_start(self, total_frags, codec, sample_hz, duration_ms, total_size):
        """Send AUDIO_START packet"""
        if not self.initialized:
            return False
            
        print("[LoRa] AUDIO_START sent")
        self.packets_sent.append({
            'type': 'START',
            'seq': self.seq_num,
            'total_frags': total_frags,
            'codec': codec,
            'sample_hz': sample_hz,
            'duration_ms': duration_ms,
            'total_size': total_size
        })
        self.seq_num += 1
        return True
    
    def send_audio_data(self, data, length):
        """Send AUDIO_DATA packet"""
        if not self.initialized:
            return False
            
        print(f"[LoRa] AUDIO_DATA seq={self.seq_num} len={length} sent")
        self.packets_sent.append({
            'type': 'DATA',
            'seq': self.seq_num,
            'length': length,
            'data': data[:length] if data else None
        })
        self.seq_num += 1
        self.total_bytes_sent += length
        return True
    
    def send_audio_end(self, frag_count, full_crc32):
        """Send AUDIO_END packet"""
        if not self.initialized:
            return False
            
        print("[LoRa] AUDIO_END sent")
        self.packets_sent.append({
            'type': 'END',
            'seq': self.seq_num,
            'frag_count': frag_count,
            'crc32': full_crc32
        })
        self.seq_num += 1
        return True
    
    def wait_for_ack(self, expected_seq, timeout_ms):
        """Wait for ACK (simulates successful ACK)"""
        print(f"[LoRa] ACK received for seq={expected_seq}")
        return True
    
    def get_last_rssi(self):
        """Get RSSI of last packet"""
        return -65  # Good signal
    
    def get_last_snr(self):
        """Get SNR of last packet"""
        return 8.5  # Good SNR
    
    def get_packets_sent(self):
        """Get all sent packets"""
        return self.packets_sent.copy()


class SystemTest:
    """Test runner for LoRa OK / SD Fail scenario"""
    
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
            print(f"  [PASS] {message}")
            return TestResult.PASS
        else:
            self.tests_failed += 1
            print(f"  [FAIL] {message}")
            return TestResult.FAIL
    
    def assert_false(self, condition, message):
        """Assert that condition is false"""
        return self.assert_true(not condition, message)
    
    def test_sd_initialization_fails(self, sd):
        """Test 1: SD initialization fails"""
        print("\n[Test 1] SD Card Initialization Failure")
        result = sd.init()
        return self.assert_false(result, "SD initialization fails as expected")
    
    def test_lora_initialization_succeeds(self, lora):
        """Test 2: LoRa initializes despite SD failure"""
        print("\n[Test 2] LoRa Initialization Success")
        result = lora.init()
        return self.assert_true(result, "LoRa initializes successfully despite SD failure")
    
    def test_lora_transmission_without_logging(self, sd, lora):
        """Test 3: LoRa can transmit when SD logging fails"""
        print("\n[Test 3] LoRa Transmission Without SD Logging")
        
        # Try to send via LoRa (should succeed)
        lora_success = lora.send_audio_start(5, 0x00, 8000, 64, 512)
        self.assert_true(lora_success, "LoRa transmission succeeds")
        
        # Try to log (should fail gracefully)
        tx_time = 1000
        ack_time = 1050
        log_success = sd.log_transmission(0.0, 0.0, tx_time, ack_time, -65, 8.5)
        self.assert_false(log_success, "SD logging fails as expected")
        
        # Verify transmission actually happened
        packets = lora.get_packets_sent()
        return self.assert_true(len(packets) == 1, 
                               f"Transmission completed despite logging failure ({len(packets)} packets)")
    
    def test_full_transmission_sequence(self, sd, lora):
        """Test 4: Complete audio transmission without SD"""
        print("\n[Test 4] Full Transmission Sequence Without SD")
        
        # Simulate full transmission
        total_frags = 3
        demo_audio = bytes([i % 256 for i in range(384)])  # 384 bytes = 3 fragments @ 128 bytes each
        
        # Track packets before this test
        packets_before = len(lora.get_packets_sent())
        bytes_before = lora.total_bytes_sent
        
        # Send START
        start_ok = lora.send_audio_start(total_frags, 0x00, 8000, 48, len(demo_audio))
        self.assert_true(start_ok, "START packet sent")
        
        # Try to log START (fails)
        sd.log_transmission(0.0, 0.0, 1000, 1050, -65, 8.5)
        
        # Send DATA fragments
        offset = 0
        chunk_size = 128
        for frag in range(total_frags):
            chunk = min(chunk_size, len(demo_audio) - offset)
            data_ok = lora.send_audio_data(demo_audio[offset:offset+chunk], chunk)
            if not data_ok:
                print(f"  [WARN] Fragment {frag} failed")
            offset += chunk
        
        # Send END
        end_ok = lora.send_audio_end(total_frags, 0x12345678)
        self.assert_true(end_ok, "END packet sent")
        
        # Verify all packets sent in this test
        packets_after = len(lora.get_packets_sent())
        new_packets = packets_after - packets_before
        expected_packets = 1 + total_frags + 1  # START + DATA + END
        self.assert_true(new_packets == expected_packets,
                        f"All packets transmitted ({new_packets}/{expected_packets})")
        
        # Verify data integrity
        bytes_sent = lora.total_bytes_sent - bytes_before
        return self.assert_true(bytes_sent == len(demo_audio),
                               f"All audio bytes transmitted ({bytes_sent}/{len(demo_audio)})")
    
    def test_multiple_transmissions(self, sd, lora):
        """Test 5: Multiple transmissions with continuous SD failure"""
        print("\n[Test 5] Multiple Transmissions With SD Failed")
        
        initial_packet_count = len(lora.get_packets_sent())
        
        # Perform 3 transmission cycles
        for i in range(3):
            self.log(f"Transmission cycle {i+1}")
            lora.send_audio_start(2, 0x00, 8000, 32, 256)
            lora.send_audio_data(bytes([0] * 128), 128)
            lora.send_audio_data(bytes([0] * 128), 128)
            lora.send_audio_end(2, 0xAABBCCDD)
            
            # SD logging fails each time
            sd.log_transmission(0.0, 0.0, 2000 + i*100, 2050 + i*100, -70, 7.0)
        
        packets = lora.get_packets_sent()
        new_packets = len(packets) - initial_packet_count
        expected = 3 * 4  # 3 cycles * 4 packets each (START + 2*DATA + END)
        
        self.assert_true(new_packets == expected,
                        f"All transmissions completed ({new_packets}/{expected} packets)")
        
        # Verify SD never logged anything
        return self.assert_true(len(sd.log_file) == 0,
                               "SD correctly blocked all logging attempts")
    
    def test_sd_write_attempts_tracked(self, sd):
        """Test 6: SD write attempts are tracked even when failing"""
        print("\n[Test 6] SD Write Attempt Tracking")
        
        attempts = sd.write_attempts
        return self.assert_true(attempts > 0,
                               f"SD write attempts tracked ({attempts} attempts made)")
    
    def test_packet_integrity(self, lora):
        """Test 7: Verify packet data integrity"""
        print("\n[Test 7] Packet Data Integrity")
        
        packets = lora.get_packets_sent()
        
        # Check START packets have required fields
        start_packets = [p for p in packets if p['type'] == 'START']
        start_ok = all('total_frags' in p and 'codec' in p for p in start_packets)
        self.assert_true(start_ok, "START packets have required fields")
        
        # Check DATA packets have data
        data_packets = [p for p in packets if p['type'] == 'DATA']
        data_ok = all(p['length'] > 0 for p in data_packets)
        self.assert_true(data_ok, "DATA packets contain data")
        
        # Check END packets have CRC
        end_packets = [p for p in packets if p['type'] == 'END']
        end_ok = all('crc32' in p for p in end_packets)
        return self.assert_true(end_ok, "END packets have CRC32")
    
    def test_system_resilience(self, sd, lora):
        """Test 8: System continues operating with degraded mode"""
        print("\n[Test 8] System Resilience (Degraded Mode)")
        
        # System should:
        # 1. Recognize SD is unavailable
        self.assert_false(sd.initialized, "System recognizes SD failure")
        
        # 2. Continue LoRa operations
        self.assert_true(lora.initialized, "LoRa remains operational")
        
        # 3. Accept but ignore log attempts
        before = len(sd.log_file)
        sd.log_transmission(0.0, 0.0, 5000, 5050, -60, 9.0)
        after = len(sd.log_file)
        self.assert_true(before == after == 0, "Log attempts fail gracefully")
        
        # 4. Still transmit successfully
        tx_ok = lora.send_audio_data(bytes([0xFF] * 100), 100)
        return self.assert_true(tx_ok, "Transmissions continue in degraded mode")
    
    def run_all_tests(self):
        """Run all test cases"""
        print("=" * 60)
        print("Test Scenario: LoRa OK, SD Card Fails")
        print("=" * 60)
        
        # Create component simulators
        sd = SDCardSimulator(init_fails=True)
        lora = LoRaSimulator()
        
        # Run tests
        self.test_sd_initialization_fails(sd)
        self.test_lora_initialization_succeeds(lora)
        self.test_lora_transmission_without_logging(sd, lora)
        self.test_full_transmission_sequence(sd, lora)
        self.test_multiple_transmissions(sd, lora)
        self.test_sd_write_attempts_tracked(sd)
        self.test_packet_integrity(lora)
        self.test_system_resilience(sd, lora)
        
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
        description="Test scenario where LoRa works but SD fails"
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
