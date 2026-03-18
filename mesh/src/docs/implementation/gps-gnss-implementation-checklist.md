# GPS/GNSS Feature and Requirements Checklist (u-blox SAM-M8Q)

## Scope
Add an optional GNSS module as a separate manager (similar to SD manager) using UART pins:
- TX: GPIO 42
- RX: GPIO 41

The system must continue operating when GNSS hardware is absent. GNSS state must be visible on display, and CSV logs must clearly represent missing coordinates.

## Feature Checklist

- [ ] Add GNSS parsing library dependency (for example TinyGPSPlus) in project configuration.
- [ ] Create a dedicated GNSS module header at src/app/sensors/GnssManager.h.
- [ ] Create a dedicated GNSS module source at src/app/sensors/GnssManager.cpp.
- [ ] Implement UART init for SAM-M8Q with TX=42 and RX=41.
- [ ] Implement a non-blocking GNSS init flow.
- [ ] Implement a GNSS update loop method to parse incoming NMEA data.
- [ ] Expose GNSS presence state (connected or not connected).
- [ ] Expose GNSS fix state (valid fix or no fix).
- [ ] Expose latest latitude/longitude values and validity state.
- [ ] Integrate GNSS manager into application setup and loop lifecycle.
- [ ] Integrate GNSS status output into display/status module.
- [ ] Integrate GNSS position retrieval into SD logging flow.
- [ ] Write numeric latitude/longitude when GNSS fix is valid.
- [ ] Write N/A placeholders when GNSS module is absent or fix is invalid.
- [ ] Add serial diagnostics for GNSS state transitions.

## Requirements Checklist

- [ ] GNSS support must be optional and must not block boot or runtime.
- [ ] Non-GNSS devices must keep all current behavior unchanged.
- [ ] Display must clearly indicate GNSS state.
- [ ] CSV schema must keep explicit latitude and longitude columns.
- [ ] CSV rows must avoid ambiguous blanks for missing GNSS data.
- [ ] Missing or invalid GNSS data must be represented as N/A (or one agreed equivalent).
- [ ] Valid GNSS data must be represented as numeric decimal coordinates.
- [ ] System must log without crash or deadlock in all GNSS states.
- [ ] Build must succeed for both GNSS-present and GNSS-absent deployments.

## Sprint-Style Integration Plan (No Timelines)

### Sprint 1 - Foundation and Module Scaffolding
- [ ] Add GNSS library dependency.
- [ ] Create GnssManager interface and implementation files.
- [ ] Implement UART wiring configuration and basic init/update APIs.
- [ ] Add compile-safe stubs for optional operation when hardware is missing.

### Sprint 2 - Runtime State and App Wiring
- [ ] Implement connected/no-fix/fix state tracking in GNSS manager.
- [ ] Wire GNSS manager into app setup/loop execution path.
- [ ] Expose GNSS state to display module.
- [ ] Render clear GNSS state indicator on screen.

### Sprint 3 - Logging Integration
- [ ] Connect SD log path to GNSS manager position API.
- [ ] Implement CSV output rules for valid coordinates vs N/A placeholders.
- [ ] Verify CSV consistency and readability for all GNSS states.

### Sprint 4 - Validation and Documentation
- [ ] Validate behavior with no GNSS connected.
- [ ] Validate behavior with GNSS connected and no satellite fix.
- [ ] Validate behavior with GNSS connected and valid position fix.
- [ ] Capture example CSV rows for each scenario.
- [ ] Update wiring and usage documentation.

## Definition of Done

- [ ] Feature checklist complete.
- [ ] Requirements checklist complete.
- [ ] Display reflects GNSS availability and fix state.
- [ ] CSV clearly differentiates valid coordinates from missing GNSS.
- [ ] End-to-end tests complete across all GNSS connection scenarios.
