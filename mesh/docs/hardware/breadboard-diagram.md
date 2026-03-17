# Mesh Node Breadboard Diagram

This document tracks the physical wiring diagram for a mesh node based on the same hardware baseline as LoRa-32-V3 plus a GPS module.

## Source of Truth

Do not redefine pin assignments here from memory.
Use the pinout artifact migrated from LoRa-32-V3 as the source of truth:

- `mesh/docs/pinout/HTIT-WB32LA(F)_V3-bd4009e3b90756e5bc8a679428950e7f.webp`

## System Wiring Diagram (Pin-Agnostic Scaffold)

```mermaid
flowchart LR
    PWR[Power Rail] --> V3[Heltec LoRa 32 V3]
    PWR --> GPS[GPS/GNSS Module]

    V3 --- OLED[OLED Display (on-board)]
    V3 --- LORA[SX1262 LoRa Radio (on-board)]
    V3 --- SD[SD Card SPI]

    GPS -->|UART TX/RX per pinout doc| V3
    SD -->|SPI bus| V3
```

## Final Wiring Checklist

- Confirm exact V3 pins for GPS TX and GPS RX from the defined pinout.
- Confirm power source and voltage compatibility for GPS module.
- Add exact wire color map and breadboard row labels.
- Add node label convention (Node A / Node B / Node C).
- Add a photo reference once assembled.
