#include <SPI.h>
#include <LoRa.h>
#include <RadioLib.h>

/* ===== RA-02 (SX1278) → ESP32 (Heltec WiFi_LoRa_32) ===== */
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // SPI bus
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  // LoRa control pins
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Start LoRa radio
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    while (true);
  }

  // Optional but recommended tuning
  LoRa.setSpreadingFactor(9);        // SF7–SF12
  LoRa.setSignalBandwidth(125E3);    // 125 kHz
  LoRa.setCodingRate4(5);            // 4/5
  LoRa.enableCrc();

  Serial.println("LoRa receiver ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: ");

    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }

    Serial.print(" | RSSI: "); // Received Signal Strength Indicator. Measured in dB, closer to 0 is a stronger signal
    Serial.print(LoRa.packetRssi());

    Serial.print(" | SNR: "); // Signal To Noise ratio. Measured in dB. The Higher it is, the better
    Serial.print(LoRa.packetSnr());

    Serial.print(" | Packet:  ");
    Serial.print(LoRa.parsePacket());

    Serial.print(" | CRC:  ");
    Serial.println(LoRa.crc());
  }
}
