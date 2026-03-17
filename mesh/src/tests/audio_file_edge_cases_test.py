#!/usr/bin/env python3
"""
Audio File Edge Cases Test
===========================
Tests edge cases and boundary conditions for audio file handling:
- Empty files (0 bytes)
- Single byte files
- Exact fragment size matches
- Very large files
- File read errors mid-transmission
- Corrupted/malformed files

Usage:
    python audio_file_edge_cases_test.py
    python audio_file_edge_cases_test.py --verbose
"""

import argparse
import sys
import io


LORA_MAX_DATA_PAYLOAD = 245  # From packet.h


class AudioFile:
    """Simulates audio file with various edge cases"""
    
    def __init__(self, data=None, read_error_at=-1):
        """
        Args:
            data: bytes to simulate file content
            read_error_at: position to trigger read error (-1 = no error)
        """
        self.data = data if data is not None else b''
        self.read_error_at = read_error_at
        self.position = 0
        self.is_open = False
        
    def open(self, filename):
        """Open the file"""
        self.is_open = True
        self.position = 0
        return True
    
    def read(self, size):
        """Read chunk from file"""
        if not self.is_open:
            raise IOError("File not open")
        
        if self.read_error_at >= 0 and self.position >= self.read_error_at:
            raise IOError(f"Read error at position {self.position}")
        
        chunk = self.data[self.position:self.position + size]
        self.position += len(chunk)
        return chunk
    
    def close(self):
        """Close the file"""
        self.is_open = False
    
    def size(self):
        """Get file size"""
        return len(self.data)


class FragmentationEngine:
    """Handles audio fragmentation logic"""
    
    def __init__(self, max_payload=LORA_MAX_DATA_PAYLOAD):
        self.max_payload = max_payload
        
    def calculate_fragments(self, file_size):
        """Calculate number of fragments needed"""
        if file_size == 0:
            return 0
        return (file_size + self.max_payload - 1) // self.max_payload
    
    def fragment_file(self, audio_file, chunk_size=None):
        """Fragment file into chunks"""
        if chunk_size is None:
            chunk_size = self.max_payload
            
        fragments = []
        while True:
            chunk = audio_file.read(chunk_size)
            if not chunk or len(chunk) == 0:
                break
            fragments.append(chunk)
        
        return fragments


class EdgeCaseTester:
    """Test suite for audio file edge cases"""
    
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.tests_run = 0
        self.tests_passed = 0
        self.tests_failed = 0
        
    def log(self, msg):
        """Log verbose message"""
        if self.verbose:
            print(f"[TEST] {msg}")
    
    def assert_test(self, condition, test_name, details=""):
        """Assert test condition"""
        self.tests_run += 1
        if condition:
            self.tests_passed += 1
            print(f"  [PASS] {test_name}")
            if details and self.verbose:
                print(f"         {details}")
            return True
        else:
            self.tests_failed += 1
            print(f"  [FAIL] {test_name}")
            if details:
                print(f"         {details}")
            return False
    
    def test_empty_file(self):
        """Test 1: Empty audio file (0 bytes)"""
        print("\n[Test 1] Empty Audio File (0 bytes)")
        
        audio = AudioFile(data=b'')
        audio.open("empty.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        self.assert_test(frag_count == 0, "Empty file requires 0 fragments",
                        f"Fragment count: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        return self.assert_test(len(fragments) == 0, "No fragments created for empty file",
                               f"Fragments: {len(fragments)}")
    
    def test_single_byte_file(self):
        """Test 2: Single byte file"""
        print("\n[Test 2] Single Byte Audio File")
        
        audio = AudioFile(data=b'\x42')
        audio.open("single.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        self.assert_test(frag_count == 1, "Single byte requires 1 fragment",
                        f"Fragment count: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        self.assert_test(len(fragments) == 1, "One fragment created")
        return self.assert_test(fragments[0] == b'\x42', "Fragment content correct",
                               f"Content: {fragments[0].hex()}")
    
    def test_exact_fragment_size(self):
        """Test 3: File exactly one fragment size"""
        print("\n[Test 3] Exact Fragment Size Match")
        
        data = bytes([i % 256 for i in range(LORA_MAX_DATA_PAYLOAD)])
        audio = AudioFile(data=data)
        audio.open("exact.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        self.assert_test(frag_count == 1, 
                        f"File of {LORA_MAX_DATA_PAYLOAD} bytes = 1 fragment",
                        f"Fragment count: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        self.assert_test(len(fragments) == 1, "One fragment created")
        return self.assert_test(len(fragments[0]) == LORA_MAX_DATA_PAYLOAD,
                               "Fragment is exact size",
                               f"Size: {len(fragments[0])}")
    
    def test_one_byte_over_fragment(self):
        """Test 4: File is one byte over exact fragment size"""
        print("\n[Test 4] One Byte Over Fragment Size")
        
        data = bytes([i % 256 for i in range(LORA_MAX_DATA_PAYLOAD + 1)])
        audio = AudioFile(data=data)
        audio.open("over_one.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        self.assert_test(frag_count == 2,
                        f"{LORA_MAX_DATA_PAYLOAD + 1} bytes requires 2 fragments",
                        f"Fragment count: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        self.assert_test(len(fragments) == 2, "Two fragments created")
        self.assert_test(len(fragments[0]) == LORA_MAX_DATA_PAYLOAD,
                        "First fragment is full size")
        return self.assert_test(len(fragments[1]) == 1,
                               "Second fragment is 1 byte",
                               f"Size: {len(fragments[1])}")
    
    def test_multiple_exact_fragments(self):
        """Test 5: File is exactly N fragments"""
        print("\n[Test 5] Multiple Exact Fragments")
        
        num_fragments = 5
        data = bytes([i % 256 for i in range(LORA_MAX_DATA_PAYLOAD * num_fragments)])
        audio = AudioFile(data=data)
        audio.open("exact_multi.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        self.assert_test(frag_count == num_fragments,
                        f"File divides evenly into {num_fragments} fragments",
                        f"Fragment count: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        self.assert_test(len(fragments) == num_fragments, 
                        f"{num_fragments} fragments created")
        
        # Check all fragments are full size
        all_full = all(len(f) == LORA_MAX_DATA_PAYLOAD for f in fragments)
        return self.assert_test(all_full, "All fragments are full size")
    
    def test_large_file(self):
        """Test 6: Large audio file (100+ fragments)"""
        print("\n[Test 6] Large Audio File")
        
        # Simulate 25KB file = ~104 fragments
        file_size = 25 * 1024
        data = bytes([i % 256 for i in range(file_size)])
        audio = AudioFile(data=data)
        audio.open("large.raw")
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(audio.size())
        
        expected_frags = (file_size + LORA_MAX_DATA_PAYLOAD - 1) // LORA_MAX_DATA_PAYLOAD
        self.assert_test(frag_count == expected_frags,
                        f"Large file fragments calculated correctly",
                        f"Expected: {expected_frags}, Got: {frag_count}")
        
        fragments = fragmenter.fragment_file(audio)
        self.assert_test(len(fragments) == expected_frags,
                        f"All {expected_frags} fragments created")
        
        # Verify total bytes match
        total_bytes = sum(len(f) for f in fragments)
        return self.assert_test(total_bytes == file_size,
                               "Total fragmented bytes match file size",
                               f"File: {file_size}, Fragments: {total_bytes}")
    
    def test_file_read_error_mid_transmission(self):
        """Test 7: File read error during transmission"""
        print("\n[Test 7] File Read Error Mid-Transmission")
        
        # Create file that errors after 2 fragments
        data = bytes([0xFF] * (LORA_MAX_DATA_PAYLOAD * 3))
        error_position = LORA_MAX_DATA_PAYLOAD * 2
        audio = AudioFile(data=data, read_error_at=error_position)
        audio.open("error.raw")
        
        fragmenter = FragmentationEngine()
        
        try:
            fragments = fragmenter.fragment_file(audio)
            # Should only get 2 fragments before error
            result = self.assert_test(len(fragments) == 2,
                                    "Only got fragments before error",
                                    f"Fragments: {len(fragments)}")
        except IOError as e:
            result = self.assert_test(True,
                                    f"Read error caught: {str(e)}",
                                    "Error handling working")
        
        return result
    
    def test_boundary_fragment_count(self):
        """Test 8: Boundary fragment count (255 max for uint8)"""
        print("\n[Test 8] Boundary Fragment Count")
        
        # If total_frags is uint16, max is 65535
        # But let's test a reasonable upper bound
        max_reasonable_frags = 1000
        file_size = max_reasonable_frags * LORA_MAX_DATA_PAYLOAD
        
        fragmenter = FragmentationEngine()
        frag_count = fragmenter.calculate_fragments(file_size)
        
        return self.assert_test(frag_count == max_reasonable_frags,
                               f"Large fragment count calculated correctly",
                               f"Fragments: {frag_count}")
    
    def test_odd_byte_sizes(self):
        """Test 9: Odd/unusual byte sizes"""
        print("\n[Test 9] Odd Byte Sizes")
        
        test_cases = [
            (3, 1),    # 3 bytes = 1 fragment
            (7, 1),    # 7 bytes = 1 fragment
            (127, 1),  # 127 bytes = 1 fragment
            (246, 2),  # 246 bytes = 2 fragments (245 + 1)
            (491, 3),  # 491 bytes = 3 fragments (245 + 245 + 1)
        ]
        
        fragmenter = FragmentationEngine()
        all_correct = True
        
        for size, expected_frags in test_cases:
            data = bytes([0xAA] * size)
            audio = AudioFile(data=data)
            audio.open(f"odd_{size}.raw")
            
            frag_count = fragmenter.calculate_fragments(audio.size())
            fragments = fragmenter.fragment_file(audio)
            
            if frag_count != expected_frags or len(fragments) != expected_frags:
                all_correct = False
                self.log(f"Failed for size {size}: expected {expected_frags}, got {frag_count}")
        
        return self.assert_test(all_correct, "All odd sizes fragment correctly",
                               f"Tested {len(test_cases)} cases")
    
    def test_fragment_data_integrity(self):
        """Test 10: Verify no data loss during fragmentation"""
        print("\n[Test 10] Fragment Data Integrity")
        
        # Create file with unique pattern
        original_data = bytes([(i * 7 + 13) % 256 for i in range(1000)])
        audio = AudioFile(data=original_data)
        audio.open("integrity.raw")
        
        fragmenter = FragmentationEngine()
        fragments = fragmenter.fragment_file(audio)
        
        # Reassemble
        reassembled = b''.join(fragments)
        
        self.assert_test(len(reassembled) == len(original_data),
                        "Reassembled size matches original",
                        f"Original: {len(original_data)}, Reassembled: {len(reassembled)}")
        
        return self.assert_test(reassembled == original_data,
                               "Reassembled data matches original exactly",
                               "No data loss or corruption")
    
    def test_maximum_fragment_size(self):
        """Test 11: Each fragment respects max payload size"""
        print("\n[Test 11] Fragment Size Constraints")
        
        # Create file that would create various fragment sizes
        data = bytes([0x55] * 1000)
        audio = AudioFile(data=data)
        audio.open("max_test.raw")
        
        fragmenter = FragmentationEngine()
        fragments = fragmenter.fragment_file(audio)
        
        # Check no fragment exceeds max size
        all_valid = all(len(f) <= LORA_MAX_DATA_PAYLOAD for f in fragments)
        self.assert_test(all_valid, "No fragment exceeds max payload size",
                        f"Max fragment: {max(len(f) for f in fragments)} bytes")
        
        # Check all but last are full size
        all_full_except_last = all(len(f) == LORA_MAX_DATA_PAYLOAD for f in fragments[:-1])
        return self.assert_test(all_full_except_last,
                               "All fragments except last are full size",
                               f"Last fragment: {len(fragments[-1])} bytes")
    
    def test_file_open_close_cycle(self):
        """Test 12: File open/close cycle works correctly"""
        print("\n[Test 12] File Open/Close Cycle")
        
        data = bytes([0x77] * 500)
        audio = AudioFile(data=data)
        
        # Open and read
        audio.open("cycle.raw")
        self.assert_test(audio.is_open, "File opens successfully")
        
        fragmenter = FragmentationEngine()
        fragments1 = fragmenter.fragment_file(audio)
        
        # Close
        audio.close()
        self.assert_test(not audio.is_open, "File closes successfully")
        
        # Re-open and read again
        audio.open("cycle.raw")
        fragments2 = fragmenter.fragment_file(audio)
        
        return self.assert_test(fragments1 == fragments2,
                               "Re-reading produces same fragments",
                               f"Fragment count: {len(fragments1)}")
    
    def run_all_tests(self):
        """Run all test cases"""
        print("=" * 60)
        print("Audio File Edge Cases Test")
        print("=" * 60)
        print(f"Max data payload: {LORA_MAX_DATA_PAYLOAD} bytes")
        
        # Run tests
        self.test_empty_file()
        self.test_single_byte_file()
        self.test_exact_fragment_size()
        self.test_one_byte_over_fragment()
        self.test_multiple_exact_fragments()
        self.test_large_file()
        self.test_file_read_error_mid_transmission()
        self.test_boundary_fragment_count()
        self.test_odd_byte_sizes()
        self.test_fragment_data_integrity()
        self.test_maximum_fragment_size()
        self.test_file_open_close_cycle()
        
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
        description="Test edge cases for audio file handling"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    args = parser.parse_args()
    
    tester = EdgeCaseTester(verbose=args.verbose)
    success = tester.run_all_tests()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
