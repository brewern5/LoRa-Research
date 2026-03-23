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
// Default uses Heltec V3 BOOT button (GPIO0, active LOW).
#ifndef MESH_TX_BUTTON_PIN
#define MESH_TX_BUTTON_PIN 0
#endif

#ifndef MESH_TX_BUTTON_ACTIVE_LOW
#define MESH_TX_BUTTON_ACTIVE_LOW 1
#endif
