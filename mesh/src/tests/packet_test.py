#!/usr/bin/env python3
"""
LoRa Audio Packet — Python Test & Validation Script
====================================================
Simulates the sender's packet building logic, validates byte layout,
and can parse Serial CSV logs from the receiver for analysis.

Usage:
    python lora_audio_test.py                  # run all tests
    python lora_audio_test.py --log rx_log.txt # parse a receiver log file
"""

import struct
import zlib
import argparse
import os
import csv
from io import StringIO

# ============================================================
#  Constants — must match LoRaAudioPacket.h
# ============================================================
LORA_PROTOCOL_VERSION  = 0x01
LORA_HEADER_SIZE       = 10
LORA_MAX_PAYLOAD       = 255
LORA_MAX_DATA_PAYLOAD  = LORA_MAX_PAYLOAD - LORA_HEADER_SIZE  # 245

PKT_AUDIO_START        = 0x01
PKT_AUDIO_DATA         = 0x02
PKT_AUDIO_END          = 0x03
PKT_ACK                = 0x04

CODEC_RAW_PCM          = 0x00
CODEC_COMPRESSED       = 0x01

FLAG_FEC_ENABLED       = (1 << 7)
FLAG_RETRANSMIT        = (1 << 6)
FLAG_LAST_FRAG         = (1 << 5)

NODE_BROADCAST         = 0xFF

CODEC_NAMES = {
    CODEC_RAW_PCM:    "Raw PCM",
    CODEC_COMPRESSED: "Compressed",
}

PKT_NAMES = {
    PKT_AUDIO_START: "AUDIO_START",
    PKT_AUDIO_DATA:  "AUDIO_DATA",
    PKT_AUDIO_END:   "AUDIO_END",
    PKT_ACK:         "ACK",
}

# ============================================================
#  CRC Helpers  (match the Arduino implementations exactly)
# ============================================================

def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE — matches the Arduino crc16() in LoRaAudioPacket.h"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc


def crc32(data: bytes) -> int:
    """CRC-32 — matches the Arduino crc32() in LoRaAudioPacket.h"""
    return zlib.crc32(data) & 0xFFFFFFFF


# ============================================================
#  Packet Building  (mirrors Arduino buildHeader + serialize functions)
# ============================================================

def make_ver_type(version: int, pkt_type: int) -> int:
    return ((version & 0x0F) << 4) | (pkt_type & 0x0F)


def get_version(ver_type: int) -> int:
    return (ver_type >> 4) & 0x0F


def get_type(ver_type: int) -> int:
    return ver_type & 0x0F


def make_sf_cr(sf: int, cr: int) -> int:
    return ((sf & 0x0F) << 4) | (cr & 0x0F)


def get_sf(sf_cr: int) -> int:
    return (sf_cr >> 4) & 0x0F


def get_cr(sf_cr: int) -> int:
    return sf_cr & 0x0F


def build_header(pkt_type: int, src: int, dst: int, exp_id: int,
                 session: int, seq: int, tx_pow: int, sf: int, cr: int) -> bytes:
    """
    Serialize a LoRaHeader into 10 bytes (little-endian, matches ESP32/Arduino).

    Layout:
      [0]    ver_type  (VERSION high nibble | TYPE low nibble)
      [1]    src_id
      [2]    dst_id
      [3]    exp_id
      [4-5]  session_id  (uint16 little-endian)
      [6-7]  seq_num     (uint16 little-endian)
      [8]    tx_pow
      [9]    sf_cr       (SF high nibble | CR low nibble)
    """
    ver_type = make_ver_type(LORA_PROTOCOL_VERSION, pkt_type)
    sf_cr    = make_sf_cr(sf, cr)
    return struct.pack('<BBBBHHBB',
        ver_type,
        src,
        dst,
        exp_id,
        session,
        seq,
        tx_pow,
        sf_cr,
    )


def build_audio_start(total_frags: int, codec_id: int, sample_hz: int,
                      duration_ms: int, total_size: int, crc16_val: int) -> bytes:
    """
    Serialize AudioStartPayload into bytes.

    Layout:
      [0-1]  total_frags  (uint16)
      [2]    codec_id     (uint8)
      [3-4]  sample_hz    (uint16)
      [5-6]  duration_ms  (uint16)
      [7-10] total_size   (uint32)
      [11-12] crc16       (uint16)
    """
    return struct.pack('<HBHHIH',
        total_frags,
        codec_id,
        sample_hz,
        duration_ms,
        total_size,
        crc16_val,
    )


def build_audio_end(frag_count: int, crc32_val: int) -> bytes:
    """
    Serialize AudioEndPayload into bytes.

    Layout:
      [0-1]  frag_count  (uint16)
      [2-5]  crc32       (uint32)
      [6]    reserved    (uint8)
    """
    return struct.pack('<HIB',
        frag_count,
        crc32_val,
        0x00,
    )


# ============================================================
#  Packet Parsing  (mirrors Arduino deserialize functions)
# ============================================================

def parse_header(data: bytes) -> dict:
    """Parse 10-byte header into a dict."""
    if len(data) < LORA_HEADER_SIZE:
        raise ValueError(f"Header too short: {len(data)} bytes (need {LORA_HEADER_SIZE})")
    ver_type, src, dst, exp_id, session, seq, tx_pow, sf_cr = struct.unpack_from('<BBBBHHBB', data, 0)
    return {
        'version':    get_version(ver_type),
        'type':       get_type(ver_type),
        'type_name':  PKT_NAMES.get(get_type(ver_type), f"UNKNOWN(0x{get_type(ver_type):02X})"),
        'src_id':     src,
        'dst_id':     dst,
        'exp_id':     exp_id,
        'session_id': session,
        'seq_num':    seq,
        'tx_pow':     tx_pow,
        'sf':         get_sf(sf_cr),
        'cr':         get_cr(sf_cr),
    }


def parse_audio_start(data: bytes) -> dict:
    """Parse AudioStartPayload from bytes after the header."""
    total_frags, codec_id, sample_hz, duration_ms, total_size, crc16_val = struct.unpack_from('<HBHHIH', data, 0)
    return {
        'total_frags':  total_frags,
        'codec_id':     codec_id,
        'codec_name':   CODEC_NAMES.get(codec_id, f"UNKNOWN(0x{codec_id:02X})"),
        'sample_hz':    sample_hz,
        'duration_ms':  duration_ms,
        'total_size':   total_size,
        'crc16':        crc16_val & 0xFFFF,
    }


def parse_audio_end(data: bytes) -> dict:
    """Parse AudioEndPayload from bytes after the header."""
    frag_count, crc32_val, reserved = struct.unpack_from('<HIB', data, 0)
    return {
        'frag_count': frag_count,
        'crc32':      crc32_val,
        'reserved':   reserved,
    }


# ============================================================
#  Packet Simulation  (full sender simulation)
# ============================================================

def simulate_transfer(audio_data: bytes, codec_id: int = CODEC_RAW_PCM,
                      sample_hz: int = 16000, duration_ms: int = 1000,
                      exp_id: int = 1, session_id: int = 0x0042,
                      src: int = 0x01, dst: int = 0x02,
                      tx_pow: int = 14, sf: int = 7, cr: int = 5) -> list[bytes]:
    """
    Simulate the full sender packet sequence for a given audio buffer.
    Returns a list of raw packet byte strings in transmission order.
    """
    total_size  = len(audio_data)
    total_frags = (total_size + LORA_MAX_DATA_PAYLOAD - 1) // LORA_MAX_DATA_PAYLOAD
    c16         = crc16(audio_data)
    c32         = crc32(audio_data)

    packets = []

    # --- START packet ---
    hdr     = build_header(PKT_AUDIO_START, src, dst, exp_id, session_id, 0, tx_pow, sf, cr)
    payload = build_audio_start(total_frags, codec_id, sample_hz, duration_ms, total_size, c16)
    packets.append(hdr + payload)

    # --- DATA packets ---
    seq    = 0
    offset = 0
    while offset < total_size:
        chunk     = audio_data[offset : offset + LORA_MAX_DATA_PAYLOAD]
        hdr       = build_header(PKT_AUDIO_DATA, src, dst, exp_id, session_id, seq, tx_pow, sf, cr)
        packets.append(hdr + chunk)
        offset   += len(chunk)
        seq      += 1

    # --- END packet ---
    hdr     = build_header(PKT_AUDIO_END, src, dst, exp_id, session_id, seq, tx_pow, sf, cr)
    payload = build_audio_end(seq, c32)
    packets.append(hdr + payload)

    return packets


# ============================================================
#  Airtime Estimation
#  Based on Semtech SX1262 LoRa airtime formula
# ============================================================

def estimate_airtime_ms(payload_bytes: int, sf: int, bw_khz: float = 125.0, cr: int = 5) -> float:
    """
    Estimate LoRa packet airtime in milliseconds.
    Assumes explicit header mode, low data rate optimisation auto.
    """
    bw_hz       = bw_khz * 1000
    t_sym       = (2 ** sf) / bw_hz * 1000  # ms per symbol
    preamble    = 8                           # standard preamble length
    t_preamble  = (preamble + 4.25) * t_sym

    # Symbol count for payload
    de = 1 if (sf >= 11 and bw_khz == 125.0) else 0  # low data rate optimisation
    n_payload = max(
        8 + max(
            (8 * payload_bytes - 4 * sf + 28 + 16 - 20) / (4 * (sf - 2 * de)),
            0
        ) * (cr + 4),
        0
    )
    t_payload = n_payload * t_sym
    return t_preamble + t_payload


def estimate_transfer_stats(audio_bytes: int, sf: int, bw_khz: float = 125.0, cr: int = 5) -> dict:
    """Estimate total airtime and throughput for a full audio transfer."""
    total_frags  = (audio_bytes + LORA_MAX_DATA_PAYLOAD - 1) // LORA_MAX_DATA_PAYLOAD
    start_pkt_sz = LORA_HEADER_SIZE + struct.calcsize('<HBHHIh')
    data_pkt_sz  = LORA_HEADER_SIZE + LORA_MAX_DATA_PAYLOAD
    end_pkt_sz   = LORA_HEADER_SIZE + struct.calcsize('<HIB')

    airtime_start = estimate_airtime_ms(start_pkt_sz, sf, bw_khz, cr)
    airtime_data  = estimate_airtime_ms(data_pkt_sz, sf, bw_khz, cr) * total_frags
    airtime_end   = estimate_airtime_ms(end_pkt_sz, sf, bw_khz, cr)
    total_airtime = airtime_start + airtime_data + airtime_end

    return {
        'audio_bytes':    audio_bytes,
        'total_frags':    total_frags,
        'total_packets':  total_frags + 2,  # +START +END
        'airtime_ms':     total_airtime,
        'airtime_s':      total_airtime / 1000,
        'throughput_bps': (audio_bytes * 8) / (total_airtime / 1000),
    }


# ============================================================
#  Dummy Audio Generators
# ============================================================

def generate_dummy_pcm(size_bytes: int = 32000) -> bytes:
    """
    Generate dummy raw PCM data.
    Uses a simple ramp pattern (0x00..0xFF repeating) — easy to spot
    corruption or misalignment when inspecting hex output.
    """
    return bytes(i % 256 for i in range(size_bytes))


def generate_dummy_compressed(size_bytes: int = 3200) -> bytes:
    """
    Generate dummy 'compressed' data — same ramp pattern but 10x smaller,
    simulating ~10:1 compression ratio for the comparison experiment.
    """
    return bytes(i % 256 for i in range(size_bytes))


# ============================================================
#  Log Parser  (parses receiver Serial CSV output)
# ============================================================

def parse_rx_log(log_text: str) -> dict:
    """
    Parse CSV lines from the receiver Serial Monitor.

    Expected line formats:
      RX,exp_id,session_id,seq,rssi,snr,len,timestamp_ms
      SESSION_START,exp_id,session_id,total_frags,codec,sample_hz,duration_ms,total_size
      SESSION_END,exp_id,session_id,frags_received,frags_expected,crc_ok,duration_ms
    """
    sessions = {}
    fragments = []

    for line in log_text.strip().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue

        parts = line.split(',')
        record_type = parts[0]

        if record_type == 'SESSION_START' and len(parts) >= 8:
            sid = parts[2]
            sessions[sid] = {
                'exp_id':      int(parts[1]),
                'session_id':  sid,
                'total_frags': int(parts[3]),
                'codec':       int(parts[4]),
                'sample_hz':   int(parts[5]),
                'duration_ms': int(parts[6]),
                'total_size':  int(parts[7]),
                'fragments':   [],
                'rssi_values': [],
                'snr_values':  [],
            }

        elif record_type == 'RX' and len(parts) >= 8:
            sid = parts[2]
            frag = {
                'exp_id':     int(parts[1]),
                'session_id': sid,
                'seq':        int(parts[3]),
                'rssi':       float(parts[4]),
                'snr':        float(parts[5]),
                'len':        int(parts[6]),
                'timestamp':  int(parts[7]),
            }
            fragments.append(frag)
            if sid in sessions:
                sessions[sid]['fragments'].append(frag)
                sessions[sid]['rssi_values'].append(frag['rssi'])
                sessions[sid]['snr_values'].append(frag['snr'])

        elif record_type == 'SESSION_END' and len(parts) >= 7:
            sid = parts[2]
            if sid in sessions:
                sessions[sid]['frags_received'] = int(parts[3])
                sessions[sid]['frags_expected'] = int(parts[4])
                sessions[sid]['crc_ok']         = parts[5] == '1'
                duration_raw = parts[6]
                sessions[sid]['duration_ms']    = int(duration_raw) if duration_raw.isdigit() else None
                sessions[sid]['timed_out']      = duration_raw == 'TIMEOUT'

    return sessions


def print_session_report(sessions: dict):
    """Print a human-readable analysis of parsed receiver sessions."""
    if not sessions:
        print("No sessions found in log.")
        return

    for sid, s in sessions.items():
        frags_rx  = s.get('frags_received', len(s['fragments']))
        frags_exp = s.get('frags_expected', s.get('total_frags', '?'))
        loss      = 0
        loss_pct  = 0.0
        if isinstance(frags_exp, int) and frags_exp > 0:
            loss     = frags_exp - frags_rx
            loss_pct = (loss / frags_exp) * 100

        rssi_list = s['rssi_values']
        snr_list  = s['snr_values']

        print(f"\n{'='*50}")
        print(f"  Session      : {sid}")
        print(f"  Experiment   : {s['exp_id']}")
        print(f"  Codec        : {CODEC_NAMES.get(s['codec'], s['codec'])}")
        print(f"  Sample Rate  : {s['sample_hz']} Hz")
        print(f"  Audio Size   : {s['total_size']} bytes")
        print(f"  Fragments    : {frags_rx}/{frags_exp} received")
        print(f"  Packet Loss  : {loss} ({loss_pct:.1f}%)")
        print(f"  CRC OK       : {s.get('crc_ok', 'unknown')}")
        if s.get('timed_out'):
            print(f"  Status       : TIMED OUT")
        if s.get('duration_ms'):
            print(f"  Duration     : {s['duration_ms']} ms")
        if rssi_list:
            print(f"  RSSI         : min={min(rssi_list):.1f}  max={max(rssi_list):.1f}  avg={sum(rssi_list)/len(rssi_list):.1f} dBm")
        if snr_list:
            print(f"  SNR          : min={min(snr_list):.1f}  max={max(snr_list):.1f}  avg={sum(snr_list)/len(snr_list):.1f} dB")
        print(f"{'='*50}")


# ============================================================
#  Tests
# ============================================================

def test_crc():
    print("\n--- Test: CRC ---")
    data = b"Hello LoRa"
    c16  = crc16(data)
    c32  = crc32(data)
    print(f"  Data   : {data}")
    print(f"  CRC-16 : 0x{c16:04X}")
    print(f"  CRC-32 : 0x{c32:08X}")

    # Verify CRC changes when data changes
    data2 = b"Hello LoRb"
    assert crc16(data2) != c16, "CRC-16 should change with different data"
    assert crc32(data2) != c32, "CRC-32 should change with different data"

    # Verify same data gives same CRC
    assert crc16(data) == c16, "CRC-16 should be deterministic"
    assert crc32(data) == c32, "CRC-32 should be deterministic"
    print("  PASS")


def test_header_pack_unpack():
    print("\n--- Test: Header Pack/Unpack ---")
    raw = build_header(
        pkt_type=PKT_AUDIO_START,
        src=0x01, dst=0x02,
        exp_id=3,
        session=0xABCD,
        seq=42,
        tx_pow=14,
        sf=9, cr=7
    )

    print(f"  Raw bytes ({len(raw)}): {' '.join(f'{b:02X}' for b in raw)}")
    assert len(raw) == LORA_HEADER_SIZE, f"Header should be {LORA_HEADER_SIZE} bytes"

    hdr = parse_header(raw)
    print(f"  Parsed   : {hdr}")

    assert hdr['version']    == LORA_PROTOCOL_VERSION
    assert hdr['type']       == PKT_AUDIO_START
    assert hdr['src_id']     == 0x01
    assert hdr['dst_id']     == 0x02
    assert hdr['exp_id']     == 3
    assert hdr['session_id'] == 0xABCD
    assert hdr['seq_num']    == 42
    assert hdr['tx_pow']     == 14
    assert hdr['sf']         == 9
    assert hdr['cr']         == 7
    print("  PASS")


def test_audio_start_pack_unpack():
    print("\n--- Test: AudioStart Pack/Unpack ---")
    raw = build_audio_start(
        total_frags=87,
        codec_id=CODEC_RAW_PCM,
        sample_hz=16000,
        duration_ms=5000,
        total_size=32000,
        crc16_val=0xDEAD
    )
    print(f"  Raw bytes ({len(raw)}): {' '.join(f'{b:02X}' for b in raw)}")
    parsed = parse_audio_start(raw)
    print(f"  Parsed   : {parsed}")

    assert parsed['total_frags'] == 87
    assert parsed['codec_id']    == CODEC_RAW_PCM
    assert parsed['sample_hz']   == 16000
    assert parsed['duration_ms'] == 5000
    assert parsed['total_size']  == 32000
    assert parsed['crc16']       == 0xDEAD
    print("  PASS")


def test_full_simulation_raw():
    print("\n--- Test: Full Transfer Simulation (Raw PCM) ---")
    dummy = generate_dummy_pcm(32000)
    packets = simulate_transfer(dummy, codec_id=CODEC_RAW_PCM, duration_ms=1000)

    print(f"  Audio size   : {len(dummy)} bytes")
    print(f"  Total packets: {len(packets)}")
    print(f"  START packet : {len(packets[0])} bytes")
    print(f"  DATA  packet : {len(packets[1])} bytes (first)")
    print(f"  END   packet : {len(packets[-1])} bytes")

    # Verify START
    start_hdr     = parse_header(packets[0])
    start_payload = parse_audio_start(packets[0][LORA_HEADER_SIZE:])
    assert start_hdr['type']          == PKT_AUDIO_START
    assert start_payload['codec_name'] == "Raw PCM"
    print(f"  START: {start_payload}")

    # Verify DATA packets
    for i, pkt in enumerate(packets[1:-1]):
        hdr = parse_header(pkt)
        assert hdr['type']    == PKT_AUDIO_DATA
        assert hdr['seq_num'] == i

    # Verify END
    end_hdr     = parse_header(packets[-1])
    end_payload = parse_audio_end(packets[-1][LORA_HEADER_SIZE:])
    assert end_hdr['type'] == PKT_AUDIO_END
    print(f"  END: {end_payload}")

    # Verify reassembly — strip headers and stitch data back together
    reassembled = b''.join(pkt[LORA_HEADER_SIZE:] for pkt in packets[1:-1])
    reassembled = reassembled[:len(dummy)]  # trim any padding on last fragment
    assert crc32(reassembled) == end_payload['crc32'], "CRC32 mismatch after reassembly"
    assert reassembled == dummy, "Reassembled data does not match original"
    print("  Reassembly CRC32: PASS")
    print("  PASS")


def test_full_simulation_compressed():
    print("\n--- Test: Full Transfer Simulation (Compressed) ---")
    dummy_raw  = generate_dummy_pcm(32000)
    dummy_comp = generate_dummy_compressed(3200)

    raw_packets  = simulate_transfer(dummy_raw,  codec_id=CODEC_RAW_PCM)
    comp_packets = simulate_transfer(dummy_comp, codec_id=CODEC_COMPRESSED)

    raw_total_bytes  = sum(len(p) for p in raw_packets)
    comp_total_bytes = sum(len(p) for p in comp_packets)
    reduction        = (1 - comp_total_bytes / raw_total_bytes) * 100

    print(f"  Raw  : {len(raw_packets)} packets, {raw_total_bytes} bytes over the air")
    print(f"  Comp : {len(comp_packets)} packets, {comp_total_bytes} bytes over the air")
    print(f"  Reduction : {reduction:.1f}%")
    print("  PASS")


def test_airtime_comparison():
    print("\n--- Test: Airtime Comparison (SF7 vs SF12) ---")
    raw_size  = 32000
    comp_size = 3200

    for sf in [7, 9, 12]:
        raw_stats  = estimate_transfer_stats(raw_size,  sf=sf)
        comp_stats = estimate_transfer_stats(comp_size, sf=sf)
        saving     = raw_stats['airtime_s'] - comp_stats['airtime_s']
        print(f"\n  SF{sf}:")
        print(f"    Raw  PCM  : {raw_stats['total_frags']:>4} frags  {raw_stats['airtime_s']:>8.2f}s  airtime")
        print(f"    Compressed: {comp_stats['total_frags']:>4} frags  {comp_stats['airtime_s']:>8.2f}s  airtime")
        print(f"    Saving    : {saving:.2f}s  ({(saving/raw_stats['airtime_s']*100):.0f}% less airtime)")


def test_byte_layout_printout():
    print("\n--- Test: Byte Layout Printout ---")
    dummy = generate_dummy_pcm(500)
    packets = simulate_transfer(dummy, session_id=0x0042, exp_id=1)

    print(f"\n  START packet hex:")
    p = packets[0]
    for i, b in enumerate(p):
        label = ""
        if i == 0:  label = " ← ver_type"
        if i == 1:  label = " ← src_id"
        if i == 2:  label = " ← dst_id"
        if i == 3:  label = " ← exp_id"
        if i == 4:  label = " ← session_id (low byte)"
        if i == 5:  label = " ← session_id (high byte)"
        if i == 6:  label = " ← seq_num (low byte)"
        if i == 7:  label = " ← seq_num (high byte)"
        if i == 8:  label = " ← tx_pow"
        if i == 9:  label = " ← sf_cr"
        if i == 10: label = " ← total_frags (low byte)"
        if i == 11: label = " ← total_frags (high byte)"
        if i == 12: label = " ← codec_id"
        if i == 13: label = " ← sample_hz (low byte)"
        if i == 14: label = " ← sample_hz (high byte)"
        if i == 15: label = " ← duration_ms (low byte)"
        if i == 16: label = " ← duration_ms (high byte)"
        if i in (17,18,19,20): label = f" ← total_size byte {i-17}"
        if i == 21: label = " ← crc16 (low byte)"
        if i == 22: label = " ← crc16 (high byte)"
        print(f"    [{i:02d}]  0x{b:02X}  ({b:>3}){label}")


# ============================================================
#  Main
# ============================================================

def run_all_tests():
    print("=" * 50)
    print("  LoRa Audio Packet — Validation Tests")
    print("=" * 50)

    test_crc()
    test_header_pack_unpack()
    test_audio_start_pack_unpack()
    test_full_simulation_raw()
    test_full_simulation_compressed()
    test_airtime_comparison()
    test_byte_layout_printout()

    print("\n" + "=" * 50)
    print("  All tests passed.")
    print("=" * 50)


def run_log_parser(log_path: str):
    if not os.path.exists(log_path):
        print(f"[ERROR] Log file not found: {log_path}")
        return

    with open(log_path, 'r') as f:
        log_text = f.read()

    print(f"\nParsing log: {log_path}")
    sessions = parse_rx_log(log_text)
    print(f"Found {len(sessions)} session(s).")
    print_session_report(sessions)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='LoRa Audio Packet Test & Log Parser')
    parser.add_argument('--log', type=str, help='Path to receiver Serial log file (CSV)')
    args = parser.parse_args()

    if args.log:
        run_log_parser(args.log)
    else:
        run_all_tests()