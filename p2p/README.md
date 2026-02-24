# P2P LoRa Projects

This folder contains multiple point-to-point LoRa experiments.

## Primary roles

- **Main transmitter (TX):** `LoRa-32-V3/`
- **Main receiver (RX):** `LoRa-32-V2/`

Use these two as the default pair for current testing.

---

## Directory map and file descriptions

### `ESP-Lora_wifi_32-SA-02/`

- `ESP-Lora_wifi_32-SA-02.ino` — Simple LoRa **receiver** sketch (433 MHz) for Heltec WiFi LoRa 32 + RA-02 wiring test; prints incoming payload, RSSI, SNR, and CRC info to Serial.

### `Esp32-SA-02-TX/`

- `Esp32-SA-02-TX.ino` — Simple LoRa **transmitter** sketch (433 MHz) that sends periodic `"Hello RA-02"` packets for bring-up testing.
- `New Text Document.txt` — Empty placeholder text file (currently unused).

### `LoRa-32-V2/` (**Main Receiver**)

- `LoRa-32-V2.ino` — Primary RX firmware for Heltec LoRa V2; receives `AUDIO_START`, `AUDIO_DATA`, and `AUDIO_END`, reassembles payloads, validates CRC, and sends ACKs.
- `packet.h` — Protocol definitions used by V2 RX (packet structs, constants, serialization helpers, CRC16/CRC32 helpers, ACK status codes).
- `sd_card.ino` — Empty placeholder for SD-card-related logic (currently unused).
- `packet_test.py` — Python protocol test/validation script for packet generation/parsing and log analysis.
- `tests/` — Empty tests folder placeholder.

### `LoRa-32-V3/` (**Main Transmitter**)

- `LoRa-32-V3.ino` — Primary TX firmware entrypoint; initializes LoRa manager and sends a fragmented demo audio transfer (`START` → `DATA` fragments → `END`) with ACK checks.
- `sketch.yaml` — Arduino CLI profile (board FQBN, libraries, serial port, monitor settings).

#### `LoRa-32-V3/src/comms/`

- `LoraManager.h` — LoRa TX manager interface and radio/session configuration constants.
- `LoraManager.cpp` — LoRa TX implementation (radio init, header fill, send start/data/end packets, wait for ACK).

#### `LoRa-32-V3/src/models/`

- `packet.h` — Shared packet/protocol model declarations (header/payload structs and helper declarations).
- `packet.cpp` — Shared packet/protocol implementations (header builder, CRC functions, serialization/deserialization).

#### `LoRa-32-V3/src/storage/`

- `sdManager.h` — SD manager API for opening/reading audio chunks and writing CSV transmission logs.
- `sdManager.cpp` — SD manager implementation for SD init, audio chunk reads, and transmission log appends.

#### `LoRa-32-V3/tests/`

- `packet_test.py` — Python packet/protocol validation script (mirrors packet format and checks serialization/CRC behavior).

#### `LoRa-32-V3/.vscode/`

- `arduino.json` — VS Code Arduino extension build/upload/monitor settings.
- `c_cpp_properties.json` — IntelliSense include paths/defines for this board environment.

#### `LoRa-32-V3/build/`

- Arduino build output directory (generated artifacts; not hand-edited source).

---

## Recommended workflow

1. Flash `LoRa-32-V2/LoRa-32-V2.ino` on the receiver board.
2. Flash `LoRa-32-V3/LoRa-32-V3.ino` on the transmitter board.
3. Open both Serial Monitors at `115200` baud to observe transfer + ACK flow.