#include "SdManager.h"

SdManager::SdManager() : _spiSD(FSPI) {}

bool SdManager::init() {
  _ready = false;
  
  Serial.println("Starting SPI...");
  delay(100);

  if( !_spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS) ) {
    delay(250);
    Serial.println("SPI for SD init failed! (check wiring/card)");
    return false;
  } else {
    Serial.println("SPI Started");
  }

  SdSpiConfig sdConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(6), &_spiSD);

  Serial.println("Testing SD card presence...");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(100);
  digitalWrite(SD_CS, LOW);
  delay(10);
  digitalWrite(SD_CS, HIGH);
  delay(100);

  if (!_sd.begin(sdConfig)) {
    Serial.println(_sd.sdErrorCode(), HEX);
    _sd.initErrorHalt(&Serial);
    Serial.println("SD init failed - Checking Card");
    Serial.printf("SD_CS=%d SD_SCK=%d SD_MOSI=%d SD_MISO=%d\n", SD_CS, SD_SCK, SD_MOSI, SD_MISO);
    return false;
  }


  Serial.print("FAT type: ");
  Serial.println(_sd.fatType());  // Should print 32
  Serial.print("Free clusters: ");
  Serial.println(_sd.freeClusterCount());


  // Ensure the log file exists and has a header so runtime logging can't fail silently
  if (!_ensureLogFile()) {
    Serial.println("SD init OK, but log file open/create failed");
    return false;
  }
  _logFile.close();

  _ready = true;
  return true;
}
// :: is a scope resolution operator
bool SdManager::openAudioFile(const char* filename) {
  return _audioFile.open(filename, O_READ);
}

bool SdManager::readAudioChunk(AudioPacket& packet) {
  packet.bytesRead = _audioFile.read(packet.buffer, sizeof(packet.buffer));
  return packet.bytesRead > 0;
}

void SdManager::closeAudioFile() {
  _audioFile.close();
}

bool SdManager::writeLogHeader() {
  // O_EXCL ensures we only write the header if the file doesn't exist yet
  if (!_logFile.open("lora_log.csv", O_WRITE | O_CREAT | O_EXCL)) return false;
  _writeLogHeader(_logFile);
  _logFile.close();
  return true;
}

void SdManager::logTransmission(float lat, float lon, uint32_t txTime,
                                 uint32_t ackTime, int rssi, float snr) {
  if (!_ready) {
    Serial.println("Log skipped: SD not ready");
    return;
  }

  if (!_ensureLogFile()) {
    Serial.println("Log open failed");
    return;
  }

  LogRow row{
    .nowMs  = millis(),
    .txTime = txTime,
    .ackTime = ackTime,
    .rttMs  = static_cast<int32_t>(ackTime) - static_cast<int32_t>(txTime),
    .lat    = lat,
    .lon    = lon,
    .rssi   = rssi,
    .snr    = snr
  };

  _writeLogRow(_logFile, row);
  _logFile.close();

  Serial.printf("[SD] Logged tx=%lu ack=%lu rtt=%ldms RSSI=%d SNR=%.1f\n",
                 row.txTime, row.ackTime, row.rttMs, row.rssi, row.snr);
  Serial.printf("[SD] CSV: %lu,%lu,%lu,%ld,%.6f,%.6f,%d,%.1f\n",
                 row.nowMs, row.txTime, row.ackTime, row.rttMs,
                 row.lat, row.lon, row.rssi, row.snr);
}

// Helpers to keep CSV order consistent and avoid brittle manual prints
namespace {
  constexpr const char* LOG_COLUMNS[] = {
    "millis",
    "tx_time",
    "ack_time",
    "rtt_ms",
    "lat",
    "lon",
    "rssi",
    "snr"
  };
  constexpr size_t LOG_COLUMN_COUNT = sizeof(LOG_COLUMNS) / sizeof(LOG_COLUMNS[0]);
}

bool SdManager::_ensureLogFile() {
  if (!_logFile.open("lora_log.csv", O_WRITE | O_CREAT | O_APPEND)) {
    Serial.print("File open error: ");
    Serial.println(_sd.sdErrorCode(), HEX);
    Serial.print("File open error sym: ");
    _sd.printSdError(&Serial);

    // Check if we can even see the card volume
    Serial.print("Volume sectors: ");
    Serial.println(_sd.sectorsPerCluster());
    return false;
  }

  // If the file is new/empty, write the header once.
  if (_logFile.fileSize() == 0) {
    _writeLogHeader(_logFile);
  }

  _logFile.seekEnd();
  return true;
}

void SdManager::_writeLogHeader(File32& file) {
  for (size_t i = 0; i < LOG_COLUMN_COUNT; ++i) {
    file.print(LOG_COLUMNS[i]);
    file.print((i + 1 < LOG_COLUMN_COUNT) ? ',' : '\n');
  }
}

void SdManager::_writeLogRow(File32& file, const LogRow& row) {
  const float lat = row.lat;
  const float lon = row.lon;

  // Print in the exact same order as LOG_COLUMNS
  file.print(row.nowMs); file.print(',');
  file.print(row.txTime); file.print(',');
  file.print(row.ackTime); file.print(',');
  file.print(row.rttMs); file.print(',');
  file.print(lat, 6); file.print(',');
  file.print(lon, 6); file.print(',');
  file.print(row.rssi); file.print(',');
  file.println(row.snr);
}

/*
bool SdManager::printLogToSerial(size_t maxLines) {
  if (!_ready) {
    Serial.println("Log print skipped: SD not ready");
    return false;
  }
    

  File32 file;
  if (!file.open("lora_log.csv", O_READ)) {
    Serial.println("Log print failed: cannot open lora_log.csv");
    return false;
  }

  Serial.println("\n---- lora_log.csv ----");
  size_t lines = 0;
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    Serial.write((char)c);
    if (c == '\n') {
      lines++;
      if (maxLines > 0 && lines >= maxLines) {
        break;
      }
    }
  }
  file.close();
  Serial.println("---- end log ----\n");
  return true;
}
  */