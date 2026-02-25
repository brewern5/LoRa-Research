#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include <SPI.h>

// Heltex ESP32 LoRa V3 SDI pins
#define SD_CS 7
#define SD_MOSI 5
#define SD_MISO 6
#define SD_SCK 4

struct AudioPacket {
  uint8_t buffer[128];  // this will match the LoRa packet size
  int bytesRead;
};

class SdManager {
  public:
    SdManager();
    bool init();
    bool openAudioFile(const char* filename);
    bool readAudioChunk(AudioPacket& packet); // returns false when EOF
    void closeAudioFile();
    void getAudio();
    bool writeLogHeader();
    void logTransmission(float lat, float lon, uint32_t txTime, uint32_t ackTime, int rssi, float snr);
    //bool printLogToSerial(size_t maxLines = 0);
    bool isReady() const { return _ready; }

    private:
      struct LogRow {
        uint32_t nowMs;
        uint32_t txTime;
        uint32_t ackTime;
        int32_t  rttMs;
        float    lat;
        float    lon;
        int      rssi;
        float    snr;
      };

      bool _ensureLogFile();
      void _writeLogHeader(File32& file);
      void _writeLogRow(File32& file, const LogRow& row);

      SdFat32 _sd; // The manager for the sd card
      File32 _audioFile;
      File32 _logFile;

      // FSPI - Means Fast SPI
      SPIClass _spiSD; // This is the SPI controller for the SD card

      bool _ready = false;
};

enum LogStatus { LOG_OK, LOG_FAIL };
