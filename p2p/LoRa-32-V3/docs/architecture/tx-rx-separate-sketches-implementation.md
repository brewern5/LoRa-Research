# TX/RX Separate Sketch Implementation Plan

## Purpose

Implement two separate firmware sketches for this research project:

- `TX` firmware: transmits audio packets and handles ACK wait/retry policy.
- `RX` firmware: receives packets and sends ACKs.

This is intentionally **not** a half-duplex role-switching design. Each board is flashed with one dedicated role.

## Design Constraints

1. Two separate `.ino` entrypoints (one TX, one RX).
2. Shared protocol and core modules across both roles.
3. GPS support only in TX.
4. Different logging events per role:
   - TX logs when each packet is sent and when ACK is received/times out.
   - RX logs when each packet is received and when ACK is sent.

## Recommended Folder Layout

Arduino cannot compile two independent `setup()`/`loop()` entrypoints in the same sketch folder. To keep two true firmware images, split into two sketch directories:

```text
p2p/LoRa-32-V3/
  tx/
    tx.ino
  rx/
    rx.ino
  src/
    comms/
    display/
    models/
    storage/
    gps/        (TX-only usage)
  docs/
    architecture/
      tx-rx-separate-sketches-implementation.md
```

Notes:
- `tx/tx.ino` and `rx/rx.ino` are separate Arduino sketches.
- Both sketches include shared code from `../src/...`.
- GPS module lives in shared source tree, but only TX references it.

## Shared vs Role-Specific Modules

### Shared

- `src/models/packet.*` (packet types, CRC, serialization)
- `src/comms/LoraManager.*` (radio bring-up, send/receive primitives, ACK primitives)
- `src/storage/SdManager.*` (CSV logging + storage)
- `src/display/StatusDisplay.*` (OLED status)

### TX-only

- `src/gps/*` (or future GPS integration points)
- Transfer/session coordinator logic
- Fragment transmit scheduler and ACK timeout policy

### RX-only

- Packet receive dispatcher
- ACK response generation and send path
- Optional receive-side reassembly state

## Behavioral Split

### TX Sketch Responsibilities

1. Initialize LoRa, SD, display, GPS.
2. Create transfer session metadata (`session_id`, `seq_num`, file metadata).
3. Send in order:
   - `PKT_AUDIO_START`
   - `PKT_AUDIO_DATA` fragments
   - `PKT_AUDIO_END`
4. For each outbound packet, wait for ACK according to timeout policy.
5. Write TX log rows:
   - `EVENT=TX_SENT` when packet leaves radio call successfully.
   - `EVENT=ACK_OK` or `EVENT=ACK_TIMEOUT` per packet.

### RX Sketch Responsibilities

1. Initialize LoRa, SD, display (no GPS dependency).
2. Listen continuously for inbound packet.
3. Parse header and packet type.
4. Validate packet (type/session/seq/CRC when applicable).
5. Send ACK for handled packet.
6. Write RX log rows:
   - `EVENT=RX_RECV` when packet received.
   - `EVENT=ACK_SENT` after ACK transmit call returns success/failure status.

## Logging Schema Update

Current logging should be extended to capture role-aware events.

Recommended added columns:

- `role` (`TX` | `RX`)
- `event` (`TX_SENT`, `ACK_OK`, `ACK_TIMEOUT`, `RX_RECV`, `ACK_SENT`, `ACK_SEND_FAIL`)
- `packet_type`
- `session_id`
- `seq_num`
- `frag_index`
- `frag_len`
- `rssi`
- `snr`
- `status`
- `timestamp_ms`

This allows side-by-side comparison of TX and RX logs in post-experiment analysis.

## Configuration Impact (VS Code / Arduino)

You do **not** need to hand-edit root `.vscode` files every time you switch between TX and RX.

Recommended workflow:

1. Set up config once.
2. Switch active sketch (`tx/tx.ino` or `rx/rx.ino`) in the Arduino extension when building/uploading.
3. Only revisit config if board, library paths, or toolchain paths change.

### Preferred setup for this project (when opening folders individually)

If you open `tx/` and `rx/` as separate VS Code roots (your current workflow), then each sketch should own its config:

- `tx/.vscode/*` for TX build/upload/intellisense settings
- `rx/.vscode/*` for RX build/upload/intellisense settings

In this mode, there is no day-to-day config switching. You simply open the TX folder to build TX and open the RX folder to build RX.

Recommended shared-code layout with this workflow:

```text
LoRa-32-V3/
  shared/
    src/
      comms/
      display/
      models/
      storage/
      gps/
  tx/
    tx.ino
    .vscode/
  rx/
    rx.ino
    .vscode/
```

TX and RX both include shared modules via relative include paths (for example from `../shared/src/...`).

## `.vscode/c_cpp_properties.json`

Current include path already covers `p2p/LoRa-32-V3/**`, so shared source remains visible. After adding `tx/` and `rx/`, IntelliSense paths still work if they remain under this tree.

Potential one-time cleanup:
- Remove stale sketch-specific `compilerArgs` entries if IntelliSense keeps pointing at only one generated `*.ino.cpp` path.
- Prefer relying on `includePath` + extension-provided config over hard-coded single-sketch object paths.

## `.vscode/arduino.json`

This file supports one active sketch build context at a time. For separate TX/RX builds:

Option A (simple):
- Keep one `arduino.json` and just switch active sketch file (`tx/tx.ino` or `rx/rx.ino`) before compile/upload.
- No manual file edits per build are needed in normal use.

Option B (repeatable):
- Maintain two checked-in variants:
  - `.vscode/arduino.tx.json`
  - `.vscode/arduino.rx.json`
- Copy desired one to `.vscode/arduino.json` before build.
- This is optional and useful only if TX and RX later require different board/config options.

Option C (recommended for your workflow):
- Keep independent `arduino.json` files inside `tx/.vscode/` and `rx/.vscode/`.
- Open TX and RX folders separately in VS Code.
- No config copy/swap step required.

## `sketch.yaml`

Either:
- Keep one profile and use it for both sketches (same board/libraries), or
- Add explicit `tx` and `rx` profiles if build options diverge later.

## Step-by-Step Implementation Sequence

1. Create `tx/` and `rx/` sketch folders.
2. Move current transmit entrypoint logic into `tx/tx.ino`.
3. Build `rx/rx.ino` receive + ACK loop with role-specific logging.
4. Ensure both sketches include shared headers from `../src/...`.
5. Add/adjust `LoraManager` APIs for clean receive and ACK-send calls usable by RX sketch.
6. Extend `SdManager` log helpers for role/event fields.
7. Update VS Code Arduino config for easy TX/RX context switching.
8. Validate compile/upload independently for TX and RX.
9. Run on two boards and verify log timelines match expected event order.

## Validation Checklist

- TX sketch compiles and uploads independently.
- RX sketch compiles and uploads independently.
- TX log shows `TX_SENT` then ACK outcome for each packet.
- RX log shows `RX_RECV` then `ACK_SENT` for each handled packet.
- No GPS dependency in RX binary.
- Packet sequence/session correlation matches between TX and RX logs.

## Non-Goals

- Dynamic runtime role switching.
- Single-binary dual-role logic.
- Additional control-plane complexity beyond ACK handling required for current research phase.
