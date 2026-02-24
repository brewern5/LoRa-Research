#line 1 "C:\\Users\\bobjo\\OneDrive\\Documents\\Arduino\\LoRa-32-V3\\sdManager.h"
#pragma once
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

class sdManager {
  public:
    bool init();
    bool openAudioFile(const char* filename);
    bool readAudioChunk(AudioPacket& packet); // returns false when EOF
    void closeAudioFile();
    void getAudio();
    void writeLogHeader();
    void logTransmission(float lat, float lon, uint32_t txTime, uint32_t ackTime, int rssi, float snr);

    private:
      SdFat _sd;
      SdFile _audioFile;
      SdFile _logFile;
}

enum LogStatus { LOG_OK, LOG_FAIL };
