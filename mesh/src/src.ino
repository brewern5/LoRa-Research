#include <Arduino.h>

// Compile-time role selection for the single mesh app entry point.
#define MESH_APP_ROLE_TX 1
#define MESH_APP_ROLE_RX 2
#define MESH_APP_ROLE_HALF_DUPLEX 3

#include "mesh_role_config.h"

#if MESH_APP_ROLE == MESH_APP_ROLE_TX
#include "tx/tx.ino"
#elif MESH_APP_ROLE == MESH_APP_ROLE_RX
#include "rx/rx.ino"
#elif MESH_APP_ROLE == MESH_APP_ROLE_HALF_DUPLEX
#include "halfduplex/halfduplex.ino"
#else
#error "Invalid MESH_APP_ROLE. Use MESH_APP_ROLE_TX (1), MESH_APP_ROLE_RX (2), or MESH_APP_ROLE_HALF_DUPLEX (3)."
#endif
