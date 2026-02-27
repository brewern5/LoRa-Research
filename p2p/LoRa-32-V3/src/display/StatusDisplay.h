#pragma once

#include <SSD1306Wire.h>

// ── OLED Pin Definitions for Heltec WiFi LoRa 32 V3 ──
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define Vext 36  // OLED power control (active LOW)

/*
 * StatusDisplay - Clean C++ interface for the Heltec WiFi LoRa 32 V3 OLED
 *
 * Usage:
 *   1. Call StatusDisplay::init() once in setup()
 *   2. Call any status endpoint (setSD, setLoRa, etc.) anywhere in your code
 *   3. The display updates automatically on every set call
 *
 * Example:
 *   StatusDisplay::init();
 *   StatusDisplay::setSD(true);
 *   StatusDisplay::setLoRa(StatusDisplay::IDLE);
 *   StatusDisplay::setLoRa(StatusDisplay::TRANSMITTING);
 *   StatusDisplay::setMessage("Joined network");
 */

class StatusDisplay {
public:

  // ── LoRa States ───────────────────────────────────────────────
  enum LoRaState {
    LORA_OK_IDLE,       // Initialized, not transmitting
    LORA_TRANSMITTING,  // Currently sending a packet
    LORA_RECEIVING,     // Currently receiving a packet
    LORA_FAIL           // Failed to initialize
  };

  // ── Lifecycle ─────────────────────────────────────────────────

  /**
   * Initialize the OLED display. Call once in setup().
   * Shows a boot splash screen briefly.
   */
  static void init();

  // ── Status Endpoints ──────────────────────────────────────────

  /**
   * Update the SD card status.
   * @param ok  true = GOOD, false = FAIL
   */
  static void setSD(bool ok);

  /**
   * Update the LoRa status.
   * @param state  One of: LORA_OK_IDLE, LORA_TRANSMITTING, LORA_RECEIVING, LORA_FAIL
   */
  static void setLoRa(LoRaState state);

  /**
   * Increment the TX packet counter and briefly show TX animation.
   * Call this each time a LoRa packet is successfully sent.
   */
  static void onPacketSent();

  /**
   * Increment the RX packet counter.
   * Call this each time a LoRa packet is received.
   */
  static void onPacketReceived();

  /**
   * Display a temporary message on the bottom status line.
   * Shown until the next call to any set* method or clearMessage().
   * @param msg  Short string (fits ~21 chars at small font)
   */
  static void setMessage(const String& msg);

  /**
   * Clear the bottom status message line.
   */
  static void clearMessage();

  /**
   * Force a full redraw of the current state.
   * Useful after waking from sleep or after display glitches.
   */
  static void refresh();

private:
  static void redraw();

  static SSD1306Wire _display;

  static bool       _sdGood;
  static LoRaState  _loraState;
  static uint32_t   _txCount;
  static uint32_t   _rxCount;
  static String     _message;
};
