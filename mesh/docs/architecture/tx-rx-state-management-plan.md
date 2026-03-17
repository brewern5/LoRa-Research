# TX/RX Research Plan and Minimal FSM Checklist

- Make TX and RX truly half-duplex first.
- Keep current packet families unchanged for baseline experiments.
- Collect timing and reliability data that can later guide mesh design.
- Stay simple now, but leave clear extension points for multi-hop.

## 1) Research Objective

Build a minimal, repeatable TX/RX state model that supports controlled experiments and trustworthy measurements for:

- Packet delivery success rate
- ACK behavior and timeout sensitivity
- Latency by spreading factor and distance
- Failure modes during burst traffic

## 2) Minimal Success Criteria (Research Ready)

A test build is research ready when:

- Radio behavior is strictly half-duplex during test runs.
- State transitions are deterministic enough to reproduce results.
- Core timings are logged for each send attempt.
- Retries are bounded and counted.
- Failures are categorized (no ACK, decode error, timeout, queue empty).
- The same FSM can be reused later with mesh fields added.

## 3) Simple FSM (Keep It Small)

Use only five states for the first research phase:

- INIT
- IDLE
- TX
- WAIT_ACK
- RX

Recommended transition flow:

- INIT -> IDLE when setup is complete.
- IDLE -> TX when a payload is available.
- TX -> WAIT_ACK when packet transmit completes.
- WAIT_ACK -> IDLE when ACK is valid.
- WAIT_ACK -> TX when timeout occurs and retry budget remains.
- WAIT_ACK -> IDLE when retry budget is exhausted.
- IDLE -> RX when node is in receive window.
- RX -> IDLE after packet parse and optional ACK send.

Notes:

- Keep all nonessential logic out of ISR callbacks.
- For research mode, avoid adding extra intermediate states unless needed to answer a specific experiment question.

## 4) Research Checklist

## A. FSM and Control

- [x] Implement the five-state FSM in a shared control module.
- [x] Add a single transition handler with clear event labels.
- [x] Ensure TX and RX never overlap on the radio.

## B. Measurements

- [x] Log one row per send attempt with packet id and retry count.
- [ ] Log tx_time, ack_time, rtt_ms, and timeout outcome.
- [ ] Log receive-side parse outcome and packet type.

## C. Experiment Configuration

- [ ] Expose experiment settings: SF, bandwidth, coding rate, TX power, ACK timeout, max retries.
- [ ] Keep a run_id field to correlate logs across devices.
- [ ] Add node_id and role tags in each log record.

## D. Repeatability

- [ ] Define a fixed test script order for all runs.
- [ ] Record environment notes (location, distance, obstacles, weather).
- [ ] Use the same payload size profile across repeated runs.

## E. Mesh Extension Hooks (Lightweight)

- [ ] Reserve packet header placeholders for src, dst, hop_count, ttl.
- [ ] Add role enum placeholders: source, relay, sink.
- [ ] Keep forwarding logic as a stub only in this phase.

## 5) Implementation Plan (Research Phases)

## Phase R1: Baseline FSM and Logging

- Add minimal five-state FSM.
- Preserve current packet types and payload format.
- Add research logs for transition events and ACK outcomes.

Deliverable:
- Reproducible single-hop TX and RX traces with half-duplex confirmation.

## Phase R2: Parameter Sweep

- Run controlled sweeps over SF and ACK timeout.
- Capture success rate, retry rate, and median RTT.
- Identify unstable regions and likely causes.

Deliverable:
- Initial SF and timeout matrix for campus testing decisions.

## Phase R3: Stress and Edge Cases

- Test burst send behavior and queue pressure.
- Test no-ACK and delayed-ACK scenarios.
- Validate that logs stay complete under stress.

Deliverable:
- Failure-mode profile and recommendations for mesh-phase reliability tuning.

## Phase R4: Mesh-Prep Snapshot

- Add header placeholders and role tags without enabling forwarding.
- Confirm baseline metrics are unchanged after placeholders are added.

Deliverable:
- Stable research baseline ready for first multi-hop prototype.

## 6) Core Data to Collect Per Run

- run_id
- node_id
- role
- sf
- bandwidth
- coding_rate
- tx_power
- packet_id
- packet_type
- payload_bytes
- retry_index
- tx_time_utc
- ack_time_utc
- rtt_ms
- outcome

## 7) Suggested Initial Integration Targets

- mesh/src/src/comms for FSM and radio control flow
- mesh/src/tx for TX loop integration
- mesh/src/rx for RX loop integration
- mesh/src/src/storage for CSV logging output
- mesh/src/tests for research scenario tests

## 8) Research Risks and Controls

- Risk: too many FSM states too early can hide root causes.
  - Control: keep five-state model until data justifies more detail.

- Risk: timing noise across runs reduces comparability.
  - Control: fixed test order, fixed payload profile, and environment notes.

- Risk: adding mesh features too soon can invalidate baseline.
  - Control: keep mesh as placeholders only until baseline is stable.

## 9) Immediate Next Steps

- [x] Implement five-state FSM and transition logs.
- [ ] Run first small matrix: 2 SF values x 2 timeout values x fixed distance.
- [ ] Review retry and RTT distributions before adding any new states.
