#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include <SPI.h>

// Heltex ESP32 LoRa V3 SDI pins
#define SD_CS 34
#define SD_MOSI 35
#define SD_MISO 37
#define SD_SCK 36

struct AudioPacket {
  uint8_t buffer[128];  // generic transport chunk; payload is pre-converted .bin data
  int bytesRead;
};

class SdManager {
  public:
    SdManager();
    bool init();
    void resetSPI();
    bool openAudioFile(const char* filename);
    bool readAudioChunk(AudioPacket& packet); // returns false when EOF
    void closeAudioFile();
    bool writeBinaryFile(const char* filename, const uint8_t* data, size_t length, bool append = false);
    bool readBinaryFile(const char* filename, uint8_t* outBuffer, size_t maxLength, size_t& bytesRead);
    void getAudio();
    bool writeLogHeader();
    bool logTransmission(float lat, float lon, uint32_t txTime, uint32_t ackTime, int rssi, float snr,
           uint16_t sessionId, uint16_t seqNum, int16_t fragIndex, uint16_t fragLen,
           const char* packetType = "GENERIC", const char* status = "UNKNOWN");
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
        uint16_t sessionId;
        uint16_t seqNum;
        int16_t  fragIndex;
        uint16_t fragLen;
        const char* packetType;
        const char* status;
      };

      bool _ensureLogFile();
      void _writeLogHeader(File32& file);
      bool _writeLogRow(File32& file, const LogRow& row);

      SdFat32 _sd; // The manager for the sd card
      File32 _audioFile;
      File32 _logFile;

      // FSPI - Means Fast SPI
      SPIClass _spiSD; // This is the SPI controller for the SD card

      bool _ready = false;
      bool _logHeaderChecked = false;
};

enum LogStatus { LOG_OK, LOG_FAIL };
