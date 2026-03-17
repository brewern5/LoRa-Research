#pragma once

#include <Arduino.h>
#include <stdint.h>

enum class ResearchState : uint8_t {
  INIT,
  IDLE,
  TX,
  WAIT_ACK,
  RX
};

enum class ResearchEvent : uint8_t {
  SETUP_COMPLETE,
  PAYLOAD_AVAILABLE,
  TX_COMPLETE,
  TX_FAILED,
  ACK_VALID,
  ACK_TIMEOUT,
  RETRY_EXHAUSTED,
  RECEIVE_WINDOW,
  RX_PACKET_DONE
};

class ResearchStateMachine {
 public:
  explicit ResearchStateMachine(const char* roleLabel = "NODE");

  ResearchState state() const;
  bool transition(ResearchEvent event);

  static const char* stateName(ResearchState state);
  static const char* eventName(ResearchEvent event);

 private:
  ResearchState _state;
  const char* _roleLabel;
};
