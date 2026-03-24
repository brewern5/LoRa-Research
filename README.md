# LoRa Research

LoRa Research is an embedded systems and networking project focused on building a
reliable long-range communication stack on ESP32/Heltec LoRa hardware.

The project starts with robust point-to-point (P2P) communication for fragmented
audio/data transfer, then expands toward a multi-hop LoRa mesh network with field
testing, routing behavior, and latency/reliability analysis.

## What This Project Is Trying To Accomplish

1. Build a reliable P2P LoRa link between transmitter and receiver nodes.
2. Improve true half-duplex behavior and ACK/timeout handling for stability.
3. Integrate SD-card logging and on-device status display for field diagnostics.
4. Add GPS/GNSS data capture for real-world range and mobility testing.
5. Evolve the system into a 3+ node mesh network with per-hop forwarding.
6. Measure and compare spread factor, distance, packet success, and latency.

## Current Focus Areas

### P2P (Tested Baseline)

The current tested baseline is point-to-point transmission using
`p2p/LoRa-32-V3` (TX) and `p2p/LoRa-32-V2` (RX), including protocol and
reliability testing in each module's `tests/` folder.

Paper: [Semantic Voice Compression for LoRa Mesh Networks in
Infrastructure-Less Environments](docs/paper/paper.pdf)

Abstract (from the paper):

> Infrastructure-less environments - such as disaster zones, remote rural
> areas, and mountainous regions impose severe bandwidth and power constraints
> that prevent conventional communication systems from supporting human-scale
> interaction. While long-range, low-power technologies like LoRa offer
> extended coverage, their limited throughput (11-242 bytes per packet) renders
> traditional voice transmission impractical. However, through compression and
> reconstruction via artificial intelligence we have achieved a ~99.6%
> compression rate, changing a 946kB file into a 4kB file and effectively
> reducing our packet count from 7,391 to 32. We demonstrate this through a
> neural audio codec, EnCodec, which operates natively at the original audio
> sample rate (48 kHz) and encodes speech into a compact set of discrete token
> indices that can be transmitted over our peer-to-peer LoRa network, with the
> model pre-installed at both ends for local reconstruction. Our validation of
> this AI compression leads us to propose plans to evaluate a more sophisticated
> model: a generative neural vocoder at the receiving node reconstructs
> intelligible speech from the compressed semantic tokens, paired with a LoRa
> mesh network. This system will be tested across rural, urban, and mountainous
> environments, measuring end-to-end latency, packet loss, semantic similarity,
> mean opinion score (MOS), and energy efficiency. Compared to raw audio
> transmission and text-based alternatives, our semantic compression proposal
> aims to maintain communicative richness essential for emergency response and
> remote community connectivity. This work establishes a foundation for
> intelligent, infrastructure-independent communication systems that prioritize
> intent preservation over raw data fidelity.

### Mesh (Next Phase)

Mesh migration and architecture work under `mesh/` will test semantic
compression under multi-hop propagation to measure how additional hops affect
latency, packet delivery reliability, and end-to-end message quality.

Design/architecture and progress documentation remains under `docs/`.

## Repository Overview

- `p2p/`: Point-to-point LoRa experiments and baseline TX/RX implementations.
- `mesh/`: In-progress mesh implementation built from the P2P baseline.
- `docs/`: Architecture notes, weekly progress, and project documentation.
- `LoRa_Research/`: Additional Arduino sketch workspace for early experiments.

## Subproject READMEs

- [P2P Overview](p2p/README.md): Main entrypoint for point-to-point experiments,
	hardware roles, and baseline TX/RX workflow.
- [Mesh Overview](mesh/README.md): Plan and roadmap for migrating from P2P to a
	multi-hop mesh network with field testing goals.
- [LoRa-32-V3 TX Node](p2p/LoRa-32-V3/README.md): Detailed transmitter firmware
	architecture, module responsibilities, protocol behavior, and logging format.

## End Goal

Deliver a tested, documented LoRa communication platform that can move from
single-hop transmission to practical multi-hop field deployment, with repeatable
metrics for reliability, range, and timing.

