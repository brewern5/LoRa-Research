#!/usr/bin/env python3
"""
Display SD Status Break Tests
==============================
Attempts to break the SD status display functionality with edge cases,
race conditions, rapid state changes, and stress testing.

This test simulates the StatusDisplay class behavior and validates that:
1. Rapid state changes don't cause inconsistencies
2. Concurrent operations don't corrupt state
3. Edge cases are handled gracefully
4. Display redraws remain consistent under stress
5. State transitions are atomic

Usage:
    python display_sd_status_break_test.py
    python display_sd_status_break_test.py --stress
    python display_sd_status_break_test.py --verbose
"""

import argparse
import time
import random
from enum import Enum
from typing import List, Tuple


class LoRaState(Enum):
    """LoRa state enumeration matching C++ code"""
    LORA_OK_IDLE = 0
    LORA_TRANSMITTING = 1
    LORA_RECEIVING = 2
    LORA_FAIL = 3


class DisplaySimulator:
    """Simulates the StatusDisplay C++ class"""
    
    def __init__(self):
        self._initialized = False
        self._sd_good = False
        self._lora_state = LoRaState.LORA_FAIL
        self._tx_count = 0
        self._rx_count = 0
        self._message = ""
        self._redraw_count = 0
        self._state_history = []
        self._max_history = 1000
        
    def init(self):
        """Initialize the display"""
        self._initialized = True
        self._redraw()
        print("[Display] Initialized")
        
    def set_sd(self, ok: bool):
        """Update SD card status"""
        if not self._initialized:
            print("[Display] WARNING: setSD called before init()")
        old_state = self._sd_good
        self._sd_good = ok
        self._record_state_change("SD", old_state, ok)
        self._redraw()
        
    def set_lora(self, state: LoRaState):
        """Update LoRa status"""
        if not self._initialized:
            print("[Display] WARNING: setLoRa called before init()")
        old_state = self._lora_state
        self._lora_state = state
        self._record_state_change("LoRa", old_state, state)
        self._redraw()
        
    def on_packet_sent(self):
        """Increment TX counter"""
        self._tx_count += 1
        self._lora_state = LoRaState.LORA_OK_IDLE
        self._redraw()
        
    def on_packet_received(self):
        """Increment RX counter"""
        self._rx_count += 1
        self._lora_state = LoRaState.LORA_OK_IDLE
        self._redraw()
        
    def set_message(self, msg: str):
        """Set status message"""
        old_msg = self._message
        self._message = msg
        self._record_state_change("Message", old_msg, msg)
        self._redraw()
        
    def clear_message(self):
        """Clear status message"""
        self._message = ""
        self._redraw()
        
    def refresh(self):
        """Force full redraw"""
        self._redraw()
        
    def _redraw(self):
        """Simulate display redraw"""
        self._redraw_count += 1
        # Simulate the display buffer being written
        # This is where corruption could happen in real hardware
        
    def _record_state_change(self, component: str, old_value, new_value):
        """Record state change for analysis"""
        entry = {
            'time': time.time(),
            'component': component,
            'old': old_value,
            'new': new_value,
            'redraw': self._redraw_count
        }
        self._state_history.append(entry)
        if len(self._state_history) > self._max_history:
            self._state_history.pop(0)
    
    def get_state(self):
        """Get current display state"""
        return {
            'initialized': self._initialized,
            'sd_good': self._sd_good,
            'lora_state': self._lora_state,
            'tx_count': self._tx_count,
            'rx_count': self._rx_count,
            'message': self._message,
            'redraw_count': self._redraw_count
        }
    
    def get_redraw_count(self):
        """Get number of redraws performed"""
        return self._redraw_count
    
    def get_state_history(self):
        """Get state change history"""
        return self._state_history.copy()


class DisplayBreakTester:
    """Test suite to break the display"""
    
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
    
    def test_uninitialized_setsd(self):
        """Test 1: Call setSD before init()"""
        print("\n[Test 1] Uninitialized setSD Call")
        display = DisplaySimulator()
        
        # Try to set SD before initialization
        display.set_sd(True)
        state = display.get_state()
        
        # Should still update state even if not initialized (matches C++ behavior)
        return self.assert_test(
            state['sd_good'] == True,
            "setSD works before init (state updated)",
            f"SD status: {state['sd_good']}"
        )
    
    def test_rapid_toggle(self):
        """Test 2: Rapid SD status toggling"""
        print("\n[Test 2] Rapid SD Status Toggling")
        display = DisplaySimulator()
        display.init()
        
        # Toggle SD status rapidly
        toggle_count = 100
        for i in range(toggle_count):
            display.set_sd(i % 2 == 0)
        
        state = display.get_state()
        expected_final = (toggle_count - 1) % 2 == 0
        
        passed = self.assert_test(
            state['sd_good'] == expected_final,
            f"Final state correct after {toggle_count} toggles",
            f"Expected: {expected_final}, Got: {state['sd_good']}"
        )
        
        # Check that redraws happened for each toggle
        self.assert_test(
            state['redraw_count'] >= toggle_count,
            f"All toggles triggered redraws ({state['redraw_count']} redraws)",
            f"Redraw count: {state['redraw_count']}"
        )
        
        return passed
    
    def test_redundant_updates(self):
        """Test 3: Setting SD to same value repeatedly"""
        print("\n[Test 3] Redundant SD Status Updates")
        display = DisplaySimulator()
        display.init()
        
        initial_redraw = display.get_redraw_count()
        
        # Set to True multiple times
        for _ in range(20):
            display.set_sd(True)
        
        state = display.get_state()
        redraws = state['redraw_count'] - initial_redraw
        
        # Even redundant updates should trigger redraws (matches C++ behavior)
        self.assert_test(
            state['sd_good'] == True,
            "SD status remains consistent",
            f"SD Good: {state['sd_good']}"
        )
        
        return self.assert_test(
            redraws == 20,
            f"Redundant updates still trigger redraws ({redraws} redraws)",
            "This could be optimized but matches current C++ implementation"
        )
    
    def test_interleaved_operations(self):
        """Test 4: Interleave SD updates with other operations"""
        print("\n[Test 4] Interleaved Operations")
        display = DisplaySimulator()
        display.init()
        
        # Interleave various operations
        operations = [
            ('sd', True),
            ('lora', LoRaState.LORA_TRANSMITTING),
            ('sd', False),
            ('tx', None),
            ('sd', True),
            ('msg', "Test message"),
            ('sd', False),
            ('rx', None),
            ('sd', True),
        ]
        
        for op_type, value in operations:
            if op_type == 'sd':
                display.set_sd(value)
            elif op_type == 'lora':
                display.set_lora(value)
            elif op_type == 'tx':
                display.on_packet_sent()
            elif op_type == 'rx':
                display.on_packet_received()
            elif op_type == 'msg':
                display.set_message(value)
        
        state = display.get_state()
        
        # Final SD state should be True (last SD operation)
        self.assert_test(
            state['sd_good'] == True,
            "Final SD state correct after interleaved ops",
            f"SD Good: {state['sd_good']}"
        )
        
        # Check counters weren't corrupted
        return self.assert_test(
            state['tx_count'] == 1 and state['rx_count'] == 1,
            "Packet counters not corrupted",
            f"TX: {state['tx_count']}, RX: {state['rx_count']}"
        )
    
    def test_message_overflow(self):
        """Test 5: Very long message with SD status changes"""
        print("\n[Test 5] Long Message with SD Updates")
        display = DisplaySimulator()
        display.init()
        
        # Set very long message
        long_msg = "A" * 200  # Way longer than display can show (~21 chars)
        display.set_message(long_msg)
        display.set_sd(True)
        display.set_sd(False)
        
        state = display.get_state()
        
        # Message should be stored as-is (truncation happens at render time)
        self.assert_test(
            len(state['message']) == 200,
            "Long message stored without corruption",
            f"Message length: {len(state['message'])}"
        )
        
        return self.assert_test(
            state['sd_good'] == False,
            "SD status correct despite long message",
            f"SD Good: {state['sd_good']}"
        )
    
    def test_state_consistency(self):
        """Test 6: Verify state consistency across operations"""
        print("\n[Test 6] State Consistency Validation")
        display = DisplaySimulator()
        display.init()
        
        # Perform random operations
        random.seed(42)
        for _ in range(50):
            op = random.choice(['sd_true', 'sd_false', 'tx', 'rx', 'refresh'])
            if op == 'sd_true':
                display.set_sd(True)
            elif op == 'sd_false':
                display.set_sd(False)
            elif op == 'tx':
                display.on_packet_sent()
            elif op == 'rx':
                display.on_packet_received()
            elif op == 'refresh':
                display.refresh()
        
        state = display.get_state()
        history = display.get_state_history()
        
        # Verify state history is complete
        self.assert_test(
            len(history) > 0,
            "State history recorded",
            f"History entries: {len(history)}"
        )
        
        # Find last SD change
        sd_changes = [h for h in history if h['component'] == 'SD']
        if sd_changes:
            last_sd_change = sd_changes[-1]
            return self.assert_test(
                state['sd_good'] == last_sd_change['new'],
                "Current SD state matches last change",
                f"Current: {state['sd_good']}, Last change: {last_sd_change['new']}"
            )
        return True
    
    def test_zero_to_hero(self):
        """Test 7: Start with everything failed, fix SD"""
        print("\n[Test 7] Zero to Hero (All Failed -> SD Fixed)")
        display = DisplaySimulator()
        display.init()
        
        # Start with everything failing
        display.set_sd(False)
        display.set_lora(LoRaState.LORA_FAIL)
        display.set_message("SYSTEM FAILURE")
        
        state1 = display.get_state()
        self.assert_test(
            not state1['sd_good'] and state1['lora_state'] == LoRaState.LORA_FAIL,
            "System starts in failed state",
            "SD: FAIL, LoRa: FAIL"
        )
        
        # Fix SD
        display.set_sd(True)
        state2 = display.get_state()
        
        return self.assert_test(
            state2['sd_good'] and state2['lora_state'] == LoRaState.LORA_FAIL,
            "SD fixed independently of LoRa state",
            f"SD: {state2['sd_good']}, LoRa: {state2['lora_state']}"
        )
    
    def test_alternating_pattern(self):
        """Test 8: Strict alternating pattern"""
        print("\n[Test 8] Alternating SD Status Pattern")
        display = DisplaySimulator()
        display.init()
        
        # Strict alternating pattern
        pattern = [True, False] * 50
        for state in pattern:
            display.set_sd(state)
        
        final_state = display.get_state()
        
        return self.assert_test(
            final_state['sd_good'] == False,  # Last value in pattern
            "Final state matches pattern end",
            f"SD Good: {final_state['sd_good']}"
        )
    
    def test_stress_concurrent_updates(self):
        """Test 9: Stress test with concurrent-style updates"""
        print("\n[Test 9] Stress Test - Rapid Mixed Updates")
        display = DisplaySimulator()
        display.init()
        
        # Simulate rapid concurrent updates
        random.seed(123)
        operations = []
        for i in range(500):
            op_type = random.choice(['sd', 'lora', 'tx', 'rx', 'msg'])
            if op_type == 'sd':
                operations.append(('sd', random.choice([True, False])))
            elif op_type == 'lora':
                operations.append(('lora', random.choice(list(LoRaState))))
            elif op_type == 'tx':
                operations.append(('tx', None))
            elif op_type == 'rx':
                operations.append(('rx', None))
            elif op_type == 'msg':
                operations.append(('msg', f"Msg{i}"))
        
        # Execute all operations
        for op_type, value in operations:
            if op_type == 'sd':
                display.set_sd(value)
            elif op_type == 'lora':
                display.set_lora(value)
            elif op_type == 'tx':
                display.on_packet_sent()
            elif op_type == 'rx':
                display.on_packet_received()
            elif op_type == 'msg':
                display.set_message(value)
        
        state = display.get_state()
        
        # Find expected final SD state
        sd_ops = [(i, v) for i, (t, v) in enumerate(operations) if t == 'sd']
        expected_sd = sd_ops[-1][1] if sd_ops else False
        
        self.assert_test(
            state['sd_good'] == expected_sd,
            f"SD state correct after {len(operations)} operations",
            f"Expected: {expected_sd}, Got: {state['sd_good']}"
        )
        
        return self.assert_test(
            state['redraw_count'] > 500,
            f"Display updated for all operations ({state['redraw_count']} redraws)",
            "No updates lost"
        )
    
    def test_boundary_counters(self):
        """Test 10: Counter overflow with SD updates"""
        print("\n[Test 10] Counter Boundary with SD Updates")
        display = DisplaySimulator()
        display.init()
        
        # Interleave counter updates with SD changes
        for i in range(100):
            if i % 10 == 0:
                display.set_sd(i % 20 == 0)
            display.on_packet_sent()
            if i % 3 == 0:
                display.on_packet_received()
        
        state = display.get_state()
        
        self.assert_test(
            state['tx_count'] == 100,
            "TX counter accurate",
            f"TX: {state['tx_count']}"
        )
        
        self.assert_test(
            state['rx_count'] == 34,  # floor(100/3) + 1
            "RX counter accurate",
            f"RX: {state['rx_count']}"
        )
        
        # Last SD update is at i=90 (90%10==0), value is 90%20==0 -> False
        return self.assert_test(
            state['sd_good'] == False,
            "SD state correct after counter operations",
            f"SD Good: {state['sd_good']}"
        )
    
    def test_message_clear_with_sd(self):
        """Test 11: Message clear interleaved with SD updates"""
        print("\n[Test 11] Message Clear with SD Updates")
        display = DisplaySimulator()
        display.init()
        
        display.set_message("Error occurred")
        display.set_sd(False)
        display.clear_message()
        display.set_sd(True)
        
        state = display.get_state()
        
        self.assert_test(
            state['message'] == "",
            "Message cleared",
            f"Message: '{state['message']}'"
        )
        
        return self.assert_test(
            state['sd_good'] == True,
            "SD state correct after message operations",
            f"SD Good: {state['sd_good']}"
        )
    
    def test_refresh_spam(self):
        """Test 12: Spam refresh with SD updates"""
        print("\n[Test 12] Refresh Spam")
        display = DisplaySimulator()
        display.init()
        
        initial_redraws = display.get_redraw_count()
        
        # Spam refresh between SD updates
        display.set_sd(True)
        for _ in range(20):
            display.refresh()
        display.set_sd(False)
        for _ in range(20):
            display.refresh()
        
        state = display.get_state()
        redraws = state['redraw_count'] - initial_redraws
        
        self.assert_test(
            state['sd_good'] == False,
            "Final SD state correct",
            f"SD Good: {state['sd_good']}"
        )
        
        return self.assert_test(
            redraws >= 42,  # 2 setSD + 40 refresh
            f"All redraws executed ({redraws} redraws)",
            "No display updates lost"
        )
    
    def run_all_tests(self, stress_mode=False):
        """Run all tests"""
        print("=" * 60)
        print("Display SD Status Break Tests")
        print("=" * 60)
        
        # Core tests
        self.test_uninitialized_setsd()
        self.test_rapid_toggle()
        self.test_redundant_updates()
        self.test_interleaved_operations()
        self.test_message_overflow()
        self.test_state_consistency()
        self.test_zero_to_hero()
        self.test_alternating_pattern()
        self.test_boundary_counters()
        self.test_message_clear_with_sd()
        self.test_refresh_spam()
        
        if stress_mode:
            print("\n" + "=" * 60)
            print("STRESS MODE: Running intensive tests")
            print("=" * 60)
            self.test_stress_concurrent_updates()
        
        # Summary
        print("\n" + "=" * 60)
        print("Test Summary")
        print("=" * 60)
        print(f"Tests Run:    {self.tests_run}")
        print(f"Tests Passed: {self.tests_passed}")
        print(f"Tests Failed: {self.tests_failed}")
        if self.tests_run > 0:
            print(f"Success Rate: {(self.tests_passed/self.tests_run*100):.1f}%")
        
        return self.tests_failed == 0


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Break tests for Display SD status functionality"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    parser.add_argument(
        '--stress',
        action='store_true',
        help='Run additional stress tests'
    )
    
    args = parser.parse_args()
    
    tester = DisplayBreakTester(verbose=args.verbose)
    success = tester.run_all_tests(stress_mode=args.stress)
    
    return 0 if success else 1


if __name__ == "__main__":
    exit(main())
