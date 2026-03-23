#pragma once

// Role selection for Arduino Maker Workshop builds.
// Change this value before upload:
//   1 = TX
//   2 = RX
//   3 = HALF_DUPLEX (same firmware can RX and TX)
#ifndef MESH_APP_ROLE 
#define MESH_APP_ROLE 3
#endif

// Half-duplex test button configuration.
// Default uses GPIO48 for manual TX trigger input.
#ifndef MESH_TX_BUTTON_PIN
#define MESH_TX_BUTTON_PIN 48
#endif

#ifndef MESH_TX_BUTTON_ACTIVE_LOW
#define MESH_TX_BUTTON_ACTIVE_LOW 1
#endif

// Phase R2 (parameter sweep) experiment controls.
// Update these values between runs to build the SF/timeout matrix.
#ifndef MESH_ACK_TIMEOUT_MS
#define MESH_ACK_TIMEOUT_MS 2000
#endif

#ifndef MESH_LORA_SF
#define MESH_LORA_SF 7
#endif

#ifndef MESH_LORA_BW_KHZ
#define MESH_LORA_BW_KHZ 125.0f
#endif

#ifndef MESH_LORA_CR
#define MESH_LORA_CR 5
#endif

#ifndef MESH_LORA_TX_POWER_DBM
#define MESH_LORA_TX_POWER_DBM 14
#endif

#ifndef MESH_NODE_ID
#define MESH_NODE_ID 0x01
#endif

#ifndef MESH_PEER_NODE_ID
#define MESH_PEER_NODE_ID 0x02
#endif

#ifndef MESH_EXPERIMENT_ID
#define MESH_EXPERIMENT_ID 0x01
#endif

#ifndef MESH_RUN_ID
#define MESH_RUN_ID "R2_DEFAULT"
#endif

#ifndef MESH_LOG_ROLE
#define MESH_LOG_ROLE "HD"
#endif
