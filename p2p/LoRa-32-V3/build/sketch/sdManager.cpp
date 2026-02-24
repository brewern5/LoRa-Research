#line 1 "C:\\Users\\bobjo\\OneDrive\\Documents\\Arduino\\LoRa-32-V3\\sdManager.cpp"
#include "sdManager.h"

bool sdManager::init() {
  if (!_sd.begin(SD_CS, SD_SCK_MHZ(20))) {
    Serial.println("SD init failed!");
    return false;
  }
  return true;
}
// :: is a scope resolution operator
bool sdManager::openAudioFile(const char* filename) {
  return _audioFile.open(filename, O_READ);
}

bool sdManager::readAudioChunk(AudioPacket& packet) {
  packet.bytesRead = _audioFile.read(packet.buffer, sizeof(packet.buffer));
  return packet.bytesRead > 0;
}

void sdManager::closeAudioFile() {
  _audioFile.close();
}

bool sdManager::writeLogHeader() {
  // O_EXCL ensures we only write the header if the file doesn't exist yet
  if (!_logFile.open("lora_log.csv", O_WRITE | O_CREAT | O_EXCL)) return false;
  _logFile.println("millis,tx_time,ack_time,rtt_ms,lat,lon,rssi,snr");
  _logFile.close();
  return true;
}

void sdManager::logTransmission(float lat, float lon, uint32_t txTime,
                                 uint32_t ackTime, int rssi, float snr) {
  if (!_logFile.open("lora_log.csv", O_WRITE | O_CREAT | O_APPEND)) {
    Serial.println("Log open failed");
    return;
  }

  _logFile.print(millis());           _logFile.print(",");
  _logFile.print(txTime);             _logFile.print(",");
  _logFile.print(ackTime);            _logFile.print(",");
  _logFile.print(ackTime - txTime);   _logFile.print(",");
  _logFile.print(lat, 6);             _logFile.print(",");
  _logFile.print(lon, 6);             _logFile.print(",");
  _logFile.print(rssi);               _logFile.print(",");
  _logFile.println(snr);

  _logFile.close();
}
