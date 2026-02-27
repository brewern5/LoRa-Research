/*
 * Example usage of StatusDisplay interface
 * Drop StatusDisplay.h and StatusDisplay.cpp into your sketch folder.
 */

#include "StatusDisplay.h"
#include <SPI.h>
#include <SD.h>
#include <LoRa.h>

#define SD_CS_PIN     5
#define LORA_SS_PIN   8
#define LORA_RST_PIN  12
#define LORA_DIO0_PIN 14
#define LORA_FREQ     915E6

void setup() {
  Serial.begin(115200);

  // 1. Init the display first so every step below can report status
  StatusDisplay::init();

  // 2. Init SD card — report result via endpoint
  bool sdOk = SD.begin(SD_CS_PIN);
  StatusDisplay::setSD(sdOk);
  StatusDisplay::setMessage(sdOk ? "SD mounted OK" : "SD mount failed");
  delay(800);

  // 3. Init LoRa — report result via endpoint
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  bool loraOk = LoRa.begin(LORA_FREQ);
  StatusDisplay::setLoRa(loraOk ? StatusDisplay::LORA_OK_IDLE : StatusDisplay::LORA_FAIL);
  StatusDisplay::setMessage(loraOk ? "LoRa ready" : "LoRa init failed");
  delay(800);

  StatusDisplay::clearMessage();
}

void loop() {
  // --- Transmit example ---
  StatusDisplay::setLoRa(StatusDisplay::LORA_TRANSMITTING);

  LoRa.beginPacket();
  LoRa.print("Hello world");
  LoRa.endPacket();

  StatusDisplay::onPacketSent();          // increments TX counter, sets IDLE

  // --- Custom message example ---
  StatusDisplay::setMessage("Sent OK");
  delay(4000);
  StatusDisplay::clearMessage();
  delay(1000);
}
