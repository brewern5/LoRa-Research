#include <SPI.h>
#include <LoRa.h>

// ----------- Pin definitions for RA-02 ----------
#define LORA_SCK   19   // SPI Clock
#define LORA_MISO  20   // SPI MISO
#define LORA_MOSI  18   // SPI MOSI
#define LORA_CS    2    // SPI Chip Select
#define LORA_RST   1  // Reset pin
#define LORA_DIO0  0   // IRQ / DIO0

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize SPI bus (XIAO ESP32-C6 hardware SPI)
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  // Tell LoRa library which pins to use
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  // Initialize LoRa at 433 MHz
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  // Optional: configure spreading factor, bandwidth, coding rate
  LoRa.setSpreadingFactor(9);      // SF7-SF12
  LoRa.setSignalBandwidth(125E3);  // 125 kHz
  LoRa.setCodingRate4(5);          // 4/5
  LoRa.enableCrc();

  Serial.println("LoRa initialized successfully");
}

void loop() {
  // Send a test packet every 2 seconds
  LoRa.beginPacket();
  LoRa.print("Hello RA-02");
  LoRa.endPacket();

  Serial.println("Packet sent");
  delay(2000);
}
