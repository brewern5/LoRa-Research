#pragma once
#include <Arduino.h>
#include <SdFat.h>

// Heltex ESP32 LoRa V3 SDI pins
#define SD_CS 35
#define SD_MOSI 43
#define SD_MISO 44
#define SD_SCK 17

struct AudioPacket {
  uint8_t buffer[128];  // this will match the LoRa packet size
  int bytesRead;
};

class SdManager {
  public:
    bool init();
    bool openAudioFile(const char* filename);
    bool readAudioChunk(AudioPacket& packet); // returns false when EOF
    void closeAudioFile();
    void getAudio();
    bool writeLogHeader();
    void logTransmission(float lat, float lon, uint32_t txTime, uint32_t ackTime, int rssi, float snr);

    private:
      SdFat _sd;
      SdFile _audioFile;
      SdFile _logFile;
};

enum LogStatus { LOG_OK, LOG_FAIL };
