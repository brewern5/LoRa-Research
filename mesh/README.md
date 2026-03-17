# Mesh LoRa-Network

A LoRa pipeline that will include a multi-hop network with at a minimum of 3 LoRa Devices.

## Migration Base: LoRa-32-V3 -> mesh

The mesh implementation will be built by migrating selected files and directories from `p2p/LoRa-32-V3` into this `mesh` directory.
The hardware base remains the same as LoRa-32-V3, with an added GPS module for field testing.

### Planned Directory Migration

Copy these items as the starting baseline:

- `p2p/LoRa-32-V3/src/` -> `mesh/src/`
- `p2p/LoRa-32-V3/tests/` -> `mesh/tests/`
- `p2p/LoRa-32-V3/docs/` -> `mesh/docs/`
- `p2p/LoRa-32-V3/assets/` -> `mesh/assets/`

Keep build output folders out of migration (`build/` should be generated locally per target).

### Module Baseline To Carry Over

- `src/comms/LoRaManager` for RF setup, TX/RX flow, ACK handling, and timeout behavior
- `src/storage/SdManager` for payload reads and logging
- `src/display/StatusDisplay` for field diagnostics without serial tethering
- `src/models/packet` for packet framing and serialization

### Mesh-Specific Adaptation After Migration

- Extend packet/routing metadata for multi-hop forwarding
- Add node role behavior (source, relay, sink)
- Add per-hop reliability handling and latency logging

## Hardware and Pinout Baseline

- Hardware platform is the same as the LoRa-32-V3 implementation.
- GPS/GNSS module will be integrated on the same platform.
- Pinout is treated as strictly defined and should be sourced from the existing pinout documentation.

Pinout source artifact currently available at:

- `p2p/LoRa-32-V3/docs/pinout/HTIT-WB32LA(F)_V3-bd4009e3b90756e5bc8a679428950e7f.webp`

When migration is performed, keep an equivalent copy under `mesh/docs/pinout/` and use that as the mesh hardware reference.

## Breadboard Diagram (GPS Integration)

Use this as a project diagram scaffold without hard-coding pin numbers here. The exact pins must match the defined pinout document.

```mermaid
flowchart LR
    A[Heltec LoRa 32 V3] --- B[SX1262 LoRa Radio (on-board)]
    A --- C[OLED Display (on-board)]
    A --- D[SD Card SPI]
    A --- E[GPS/GNSS Module]

    E -->|UART TX/RX per defined pinout| A
    D -->|SPI bus| A
```

### Diagram Completion Checklist

- Add exact wire map from the defined pinout doc into `mesh/docs/hardware/`.
- Add power rail mapping for board + GPS module.
- Add label set for test-bench photos to match node IDs.

## Protocol Packet Types (Baseline)

The baseline packet families reused from LoRa-32-V3:

- `PKT_AUDIO_START`
- `PKT_AUDIO_DATA`
- `PKT_AUDIO_END`
- `PKT_ACK`

Mesh-specific packet types can be added later when routing behavior is finalized.

## Logging Format

Use the same CSV timestamp style for cross-device correlation:

- `timestamp_utc`
- `tx_time_utc`
- `ack_time_utc`

Keep device-relative timing fields for latency analysis:

- `millis`
- `tx_time`
- `ack_time`
- `rtt_ms`

## Libraries (Software Baseline)

- `SdFat` for robust file I/O and logging
- `RadioLib` for LoRa radio abstraction and timing-sensitive operations

## Development Environment

- Visual Studio Code with Arduino tooling
- Compile-time debug flags (for example `LORA_DEBUG`) for controlled diagnostics

# What Needs to happen
## 1
Effectively redesign the LoRa p2p devices to be truly half duplex as they are not fully half-duplex. This will be the first integration.

## 2
Add GPS/GNSS module to begin long-range testing.
- Integrate the module as hardware
- integrate the module through software to save the longitude/latitude
  
## 3
Begin testing within NKU's campus ( Around the campus )
- Start with range testing to figure out the longest range that certain SF's can go, which will tie into the SF Matrix.
- Test SF at different ranges and create a matrix that can easily show the differentials between them
  - This will need more thought out as close range will be more beneficial to lower SFs, so one of two things must happen:
    - Keep this matrix, keep note of that discrepency with SF, and compare to the mesh matrix.
      - My issue with this is that the mesh matrix will have more latency concerns with multi-hops, which could affect the output.
    - Test with a different range
      - This will definately have issues with lower SF ranges, but higher will be visable, which actually makes more sense to me ( However  )
## 4
Create a mesh network
- 
- Most unknown Unknowns
- Same testing approach as the previous points but with at least 3 nodes, testing latency on the final node.
-  This testing methodology will need to be more thought out

