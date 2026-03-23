#include "ResearchStateMachine.h"

ResearchStateMachine::ResearchStateMachine(const char* roleLabel)
    : _state(ResearchState::INIT), _roleLabel(roleLabel ? roleLabel : "NODE") {}

ResearchState ResearchStateMachine::state() const {
  return _state;
}

const char* ResearchStateMachine::stateName(ResearchState state) {
  switch (state) {
    case ResearchState::INIT: return "INIT";
    case ResearchState::IDLE: return "IDLE";
    case ResearchState::TX: return "TX";
    case ResearchState::WAIT_ACK: return "WAIT_ACK";
    case ResearchState::RX: return "RX";
    default: return "UNKNOWN";
  }
}

const char* ResearchStateMachine::eventName(ResearchEvent event) {
  switch (event) {
    case ResearchEvent::SETUP_COMPLETE: return "SETUP_COMPLETE";
    case ResearchEvent::PAYLOAD_AVAILABLE: return "PAYLOAD_AVAILABLE";
    case ResearchEvent::TX_COMPLETE: return "TX_COMPLETE";
    case ResearchEvent::TX_FAILED: return "TX_FAILED";
    case ResearchEvent::ACK_VALID: return "ACK_VALID";
    case ResearchEvent::ACK_TIMEOUT: return "ACK_TIMEOUT";
    case ResearchEvent::RETRY_EXHAUSTED: return "RETRY_EXHAUSTED";
    case ResearchEvent::RECEIVE_WINDOW: return "RECEIVE_WINDOW";
    case ResearchEvent::RX_PACKET_DONE: return "RX_PACKET_DONE";
    default: return "UNKNOWN";
  }
}

bool ResearchStateMachine::transition(ResearchEvent event) {
  const ResearchState from = _state;
  ResearchState to = from;

  switch (from) {
    case ResearchState::INIT:
      if (event == ResearchEvent::SETUP_COMPLETE) {
        to = ResearchState::IDLE;
      }
      break;

    case ResearchState::IDLE:
      if (event == ResearchEvent::PAYLOAD_AVAILABLE) {
        to = ResearchState::TX;
      } else if (event == ResearchEvent::RECEIVE_WINDOW) {
        to = ResearchState::RX;
      }
      break;

    case ResearchState::TX:
      if (event == ResearchEvent::TX_COMPLETE) {
        to = ResearchState::WAIT_ACK;
      } else if (event == ResearchEvent::TX_FAILED) {
        to = ResearchState::IDLE;
      }
      break;

    case ResearchState::WAIT_ACK:
      if (event == ResearchEvent::ACK_VALID) {
        to = ResearchState::IDLE;
      } else if (event == ResearchEvent::ACK_TIMEOUT) {
        to = ResearchState::TX;
      } else if (event == ResearchEvent::RETRY_EXHAUSTED) {
        to = ResearchState::IDLE;
      }
      break;

    case ResearchState::RX:
      if (event == ResearchEvent::RX_PACKET_DONE) {
        to = ResearchState::IDLE;
      }
      break;

    default:
      break;
  }

  if (to == from) {
    return false;
  }

  _state = to;
  Serial.printf("[FSM][%s] %s --(%s)--> %s\n",
                _roleLabel,
                stateName(from),
                eventName(event),
                stateName(to));
  return true;
}
