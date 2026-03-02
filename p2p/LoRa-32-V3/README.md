# LoRa-32-V3

ESP32 LoRa transmitter prototype for sending preprocessed audio payloads over a custom LoRa packet protocol.

## Hardware Overview

This folder contains the firmware and supporting files for the **Heltec ESP32 WiFi LoRa 32 V3** development board. We selected this board for our main transmitting device due to its comprehensive feature set:

- **915MHz LoRa capability** – Compatible with the 915MHz ISM band used in North America
- **Multi-SPI functionality** – Supports both the LoRa radio and SD card reader simultaneously
- **UART interface** – Reserved for GPS module integration (future implementation)
- **I2C for built-in OLED display** – Enables portable, self-contained operation with real-time status feedback

We chose the V3 over the V2 model specifically for these enhanced connectivity options and improved hardware compatibility. This device serves as our primary field testing unit and will be used for traveling experiments to evaluate LoRa range and reliability limitations in our local area.

### Folder Contents

- **LoRa-32-V3.ino** – Main Arduino sketch
- **src/** – Modular source code (communication, display, models, storage)
- **tests/** – Python and C++ test suites for validation
- **build/** – Compiled firmware and build artifacts
- **docs/** – Hardware documentation and pinout diagrams
- **assets/** – Media files (audio samples for testing)

## Module Overview

### src/comms/LoRaManager
Manages all LoRa radio operations including packet transmission, acknowledgment handling, and RF parameter configuration. Handles the SX1262 radio chip interfacing and protocol-level communication with peer nodes.

### src/display/StatusDisplay
Provides real-time status feedback through the built-in OLED display. Serves as the primary debug station when `Serial.print()` is unavailable during field testing or portable operation. Displays LoRa state, SD card status, transmission progress, and custom messages.

### src/storage/SdManager
Handles all SD card operations including audio file reading, binary file I/O, and transmission logging. Manages SPI bus sharing with the LoRa radio and provides structured logging for post-experiment analysis.

### src/models/packet
Defines the custom LoRa packet protocol structure including headers, payload types, and serialization/deserialization functions. Implements the framing logic for audio file transfers over LoRa.

## Libraries

### SdFat
We use the **SdFat** library instead of the standard SD library for superior file management and logging capabilities. SdFat provides:
- Better performance with large files
- More robust error handling
- Advanced features like timestamps and file attributes
- Improved memory efficiency for embedded systems

### RadioLib
The **RadioLib** library is used for LoRa communication due to its excellent interfacing with our ESP32 microcontroller and the SX1262 radio chip. RadioLib offers:
- Clean abstraction over low-level SPI operations
- Support for multiple LoRa chipsets with unified API
- Built-in handling of timing-critical operations
- Active development and community support

## Development Environment

This project is developed using **Visual Studio Code** with the **Arduino Maker Workshop** extension, which provides enhanced Arduino development capabilities including improved IntelliSense, debugging support, and build management.

## Important Clarifications

- Audio is converted to a `.bin` file **before** it reaches this device.
- The device reads that `.bin` payload and fragments it into LoRa protocol packets.
- The name `AudioPacket` in code is a generic container/chunk name used during file reads and packet transport.
- `AudioPacket` does **not** mean the payload is raw audio format metadata by itself; it is transport framing.

## Protocol Packet Types

Defined in [src/models/packet.h](src/models/packet.h):

- `PKT_AUDIO_START` – start metadata for the transfer
- `PKT_AUDIO_DATA` – data fragment payload
- `PKT_AUDIO_END` – transfer completion + CRC summary
- `PKT_ACK` – acknowledgement/status

## Debug Compile Flags (`#ifdef`)

`#ifdef` checks are compile-time switches based on whether a macro is defined.

Example in [src/models/packet.cpp](src/models/packet.cpp#L82):

- `#ifdef LORA_DEBUG` includes debug print functions only when `LORA_DEBUG` is defined.
- If `LORA_DEBUG` is not defined, that code is excluded from the firmware binary.

To enable this, define the macro at compile time (for example in the sketch before includes, or via build flags):

```cpp
#define LORA_DEBUG
```

Note: `#ifdef` does not evaluate runtime booleans like `LORA_DEBUG = true`; it only checks macro definition existence.
