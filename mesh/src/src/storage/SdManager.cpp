#include "SdManager.h"
#include <time.h>

SdManager::SdManager() : _spiSD(FSPI) {}

namespace {
  constexpr uint8_t kSdCmd18Error = 0x0C;
  constexpr uint8_t kLoRaNssPin = 8;

  constexpr int kIso8601BufferLen = 32;

  inline void quiesceSharedSpiDevices() {
    pinMode(kLoRaNssPin, OUTPUT);
    digitalWrite(kLoRaNssPin, HIGH);
    delayMicroseconds(5);
  }

  bool hasBinExtension(const char* filename) {
    if (!filename) return false;
    const char* dot = strrchr(filename, '.');
    if (!dot) return false;

    const bool b = (dot[1] == 'b' || dot[1] == 'B');
    const bool i = (dot[2] == 'i' || dot[2] == 'I');
    const bool n = (dot[3] == 'n' || dot[3] == 'N');
    const bool end = (dot[4] == '\0');
    return b && i && n && end;
  }

  int monthFromShortName(const char* month) {
    static const char* kMonths[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int index = 0; index < 12; ++index) {
      if (strncmp(month, kMonths[index], 3) == 0) {
        return index + 1;
      }
    }
    return 1;
  }

  int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= (month <= 2);
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
  }

  time_t compileEpochUtc() {
    char monthText[4] = {0, 0, 0, 0};
    int day = 1;
    int year = 1970;
    int hour = 0;
    int minute = 0;
    int second = 0;

    sscanf(__DATE__, "%3s %d %d", monthText, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    const int month = monthFromShortName(monthText);
    const int64_t days = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    return static_cast<time_t>(days * 86400LL + hour * 3600LL + minute * 60LL + second);
  }
}

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

  quiesceSharedSpiDevices();

  SdSpiConfig sdConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(2), &_spiSD);

  Serial.println("Testing SD card presence...");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(100);
  digitalWrite(SD_CS, LOW);
  delay(10);
  digitalWrite(SD_CS, HIGH);
  delay(100);

  if (!_sd.begin(sdConfig)) {
    Serial.print("SD Error Code - ");
    Serial.println(_sd.sdErrorCode(), HEX);
    _sd.initErrorHalt(&Serial);
    Serial.println("SD init failed - Checking Card");
    Serial.printf("SD_CS=%d SD_SCK=%d SD_MOSI=%d SD_MISO=%d\n", SD_CS, SD_SCK, SD_MOSI, SD_MISO);
    return false;
  }


  Serial.print("FAT type: ");
  Serial.println(_sd.fatType());  // Should print 32

  // Takes too long to do
  //Serial.print("Free clusters: ");
  //Serial.println(_sd.freeClusterCount());


  // Ensure the log file exists and has a header so runtime logging can't fail silently
  if (!_ensureLogFile()) {
    Serial.println("SD init OK, but log file open/create failed");
    return false;
  }
  _logHeaderChecked = true;

  _initializeTimeBase();

  _ready = true;
  return true;
}

void SdManager::resetSPI() {
  // Ensure LoRa is deselected
  pinMode(kLoRaNssPin, OUTPUT);
  digitalWrite(kLoRaNssPin, HIGH);
  delayMicroseconds(10);
  
  // Reset SPI bus
  SPI.end();
  delayMicroseconds(50);
  
  // Restart SPI for SD card
  _spiSD.end();
  delayMicroseconds(10);
  _spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delayMicroseconds(10);
  
  // Ensure SD CS is high before operations
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delayMicroseconds(5);
}

// :: is a scope resolution operator
bool SdManager::openAudioFile(const char* filename) {
  if (!_ready || !filename) {
    return false;
  }
  resetSPI();
  _audioFile.close();
  return _audioFile.open(filename, O_READ);
}

bool SdManager::readAudioChunk(AudioPacket& packet) {
  resetSPI();
  packet.bytesRead = _audioFile.read(packet.buffer, sizeof(packet.buffer));
  return packet.bytesRead > 0;
}

void SdManager::closeAudioFile() {
  _audioFile.close();
}

bool SdManager::writeBinaryFile(const char* filename, const uint8_t* data, size_t length, bool append) {
  if (!_ready || !filename || !data) {
    return false;
  }
  if (!hasBinExtension(filename)) {
    Serial.println("Binary write rejected: filename must end with .bin");
    return false;
  }

  resetSPI();
  
  File32 file;
  oflag_t flags = O_WRITE | O_CREAT;
  flags |= append ? O_APPEND : O_TRUNC;

  if (!file.open(filename, flags)) {
    Serial.print("Binary write open failed: ");
    Serial.println(filename);
    return false;
  }

  const size_t written = file.write(data, length);
  file.flush();
  file.close();

  if (written != length) {
    Serial.printf("Binary write short: wanted=%u wrote=%u\n",
                  static_cast<unsigned>(length), static_cast<unsigned>(written));
    return false;
  }

  return true;
}

bool SdManager::readBinaryFile(const char* filename, uint8_t* outBuffer, size_t maxLength, size_t& bytesRead) {
  bytesRead = 0;
  if (!_ready || !filename || !outBuffer || maxLength == 0) {
    return false;
  }
  if (!hasBinExtension(filename)) {
    Serial.println("Binary read rejected: filename must end with .bin");
    return false;
  }

  resetSPI();
  
  File32 file;
  if (!file.open(filename, O_READ)) {
    Serial.print("Binary read open failed: ");
    Serial.println(filename);
    return false;
  }

  const int readCount = file.read(outBuffer, maxLength);
  file.close();
  if (readCount < 0) {
    Serial.println("Binary read failed");
    return false;
  }

  bytesRead = static_cast<size_t>(readCount);
  return true;
}

bool SdManager::writeLogHeader() {
  resetSPI();
  // O_EXCL ensures we only write the header if the file doesn't exist yet
  if (!_logFile.open("lora_log.csv", O_WRITE | O_CREAT | O_EXCL)) return false;
  _writeLogHeader(_logFile);
  _logFile.close();
  return true;
}

bool SdManager::logTransmission(float lat, float lon, uint32_t txTime,
                                 uint32_t ackTime, int rssi, float snr,
                                 uint16_t sessionId, uint16_t seqNum, int16_t fragIndex, uint16_t fragLen,
                                 const char* packetType, const char* status) {
  if (!_ready) {
    Serial.println("Log skipped: SD not ready");
    return false;
  }

  resetSPI();
  
  if (!_ensureLogFile()) {
    Serial.println("Log open failed");
    return false;
  }

  LogRow row{
    .nowMs  = millis(),
    .txTime = txTime,
    .ackTime = ackTime,
    .rttMs  = static_cast<int32_t>(ackTime) - static_cast<int32_t>(txTime),
    .runId = MESH_RUN_ID,
    .role = MESH_LOG_ROLE,
    .nodeId = static_cast<uint8_t>(MESH_NODE_ID),
    .sf = static_cast<uint8_t>(MESH_LORA_SF),
    .ackTimeoutMs = static_cast<uint32_t>(MESH_ACK_TIMEOUT_MS),
    .lat    = lat,
    .lon    = lon,
    .rssi   = rssi,
    .snr    = snr,
    .sessionId = sessionId,
    .seqNum = seqNum,
    .fragIndex = fragIndex,
    .fragLen = fragLen,
    .packetType = packetType ? packetType : "GENERIC",
    .status = status ? status : "UNKNOWN"
  };

  quiesceSharedSpiDevices();

  bool writeOk = _writeLogRow(_logFile, row);

  if (!writeOk) {
    Serial.printf("[SD] LOG WRITE RETRY type=%s status=%s seq=%u frag=%d len=%u write=%s\n",
                  row.packetType, row.status, row.seqNum, row.fragIndex, row.fragLen,
                  writeOk ? "OK" : "FAIL");

    if (_logFile.isOpen()) {
      _logFile.close();
      digitalWrite(SD_CS, HIGH);
    }

    if (_sd.sdErrorCode() == kSdCmd18Error) {
      Serial.println("[SD] CMD18 transport error detected, remounting SD and resetting SPI...");
      resetSPI();
      quiesceSharedSpiDevices();
      SdSpiConfig recoverCfg(SD_CS, SHARED_SPI, SD_SCK_MHZ(2), &_spiSD);
      if (_sd.begin(recoverCfg)) {
        _logHeaderChecked = false;
      }
    }

    resetSPI();
    if (_ensureLogFile()) {
      writeOk = _writeLogRow(_logFile, row);
    } else {
      writeOk = false;
    }
  }

  if (!writeOk) {
    Serial.printf("[SD] LOG WRITE FAIL type=%s status=%s seq=%u frag=%d len=%u tx=%lu ack=%lu write=%s\n",
                  row.packetType, row.status, row.seqNum, row.fragIndex, row.fragLen,
                  row.txTime, row.ackTime, writeOk ? "OK" : "FAIL");
    Serial.print("[SD] Error code: ");
    Serial.println(_sd.sdErrorCode(), HEX);
    Serial.print("[SD] Error detail: ");
    _sd.printSdError(&Serial);
    return false;
  }

  Serial.printf("[SD] LOG WRITE OK type=%s status=%s sid=%u seq=%u frag=%d len=%u tx=%lu ack=%lu rtt=%ldms RSSI=%d SNR=%.1f\n",
                row.packetType, row.status, row.sessionId, row.seqNum, row.fragIndex, row.fragLen,
                row.txTime, row.ackTime, row.rttMs, row.rssi, row.snr);
  Serial.printf("[SD] CSV: %lu,%lu,%lu,%ld,%.6f,%.6f,%d,%.1f,%u,%u,%d,%u,%s,%s\n",
                row.nowMs, row.txTime, row.ackTime, row.rttMs,
                row.lat, row.lon, row.rssi, row.snr,
                row.sessionId, row.seqNum, row.fragIndex, row.fragLen, row.packetType, row.status);
  return true;
}

// Helpers to keep CSV order consistent and avoid brittle manual prints
namespace {
  constexpr const char* LOG_COLUMNS[] = {
    "timestamp_utc",
    "millis",
    "tx_time_utc",
    "tx_time",
    "ack_time_utc",
    "ack_time",
    "rtt_ms",
    "run_id",
    "role",
    "node_id",
    "sf",
    "ack_timeout_ms",
    "lat",
    "lon",
    "rssi",
    "snr",
    "session_id",
    "seq_num",
    "frag_index",
    "frag_len",
    "packet_type",
    "status"
  };
  constexpr size_t LOG_COLUMN_COUNT = sizeof(LOG_COLUMNS) / sizeof(LOG_COLUMNS[0]);
  constexpr const char* EXPECTED_LOG_HEADER =
    "timestamp_utc,millis,tx_time_utc,tx_time,ack_time_utc,ack_time,rtt_ms,run_id,role,node_id,sf,ack_timeout_ms,lat,lon,rssi,snr,session_id,seq_num,frag_index,frag_len,packet_type,status";
}

void SdManager::_initializeTimeBase() {
  const uint32_t nowMs = millis();
  const time_t nowEpoch = time(nullptr);

  if (nowEpoch > 1700000000) {
    const uint64_t nowEpochMs = static_cast<uint64_t>(nowEpoch) * 1000ULL;
    _epochBaseMs = (nowEpochMs > nowMs) ? (nowEpochMs - nowMs) : 0;
    return;
  }

  _epochBaseMs = static_cast<uint64_t>(compileEpochUtc()) * 1000ULL;
}

uint64_t SdManager::_toEpochMs(uint32_t msSinceBoot) const {
  return _epochBaseMs + static_cast<uint64_t>(msSinceBoot);
}

void SdManager::_formatIso8601(uint64_t epochMs, char* out, size_t outLen) const {
  if (!out || outLen == 0) {
    return;
  }

  const time_t epochSec = static_cast<time_t>(epochMs / 1000ULL);
  const unsigned millisPart = static_cast<unsigned>(epochMs % 1000ULL);
  struct tm utcTime;

  if (!gmtime_r(&epochSec, &utcTime)) {
    snprintf(out, outLen, "1970-01-01T00:00:00.000Z");
    return;
  }

  char dateTimePart[24];
  strftime(dateTimePart, sizeof(dateTimePart), "%Y-%m-%dT%H:%M:%S", &utcTime);
  snprintf(out, outLen, "%s.%03uZ", dateTimePart, millisPart);
}

bool SdManager::_ensureLogFile() {
  quiesceSharedSpiDevices();

  if (_logFile.isOpen()) {
    return true;
  }

  if (!_logHeaderChecked) {
    if (_sd.exists("lora_log.csv") && !_hasExpectedLogHeader()) {
      const char* legacyName = "lora_log_legacy.csv";
      if (_sd.exists(legacyName)) {
        _sd.remove(legacyName);
      }

      if (!_sd.rename("lora_log.csv", legacyName)) {
        Serial.println("[SD] Failed to rotate legacy CSV header");
        return false;
      }
      Serial.println("[SD] Rotated legacy CSV to lora_log_legacy.csv");
    }

    const bool headerCreated = writeLogHeader();
    if (headerCreated) {
      Serial.println("[SD] Created lora_log.csv with header");
    }
    _logHeaderChecked = true;
  }

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

  return true;
}

bool SdManager::_hasExpectedLogHeader() {
  File32 headerFile;
  if (!headerFile.open("lora_log.csv", O_READ)) {
    return false;
  }

  char headerLine[196];
  size_t index = 0;
  while (headerFile.available() && index < (sizeof(headerLine) - 1)) {
    const int raw = headerFile.read();
    if (raw < 0) {
      break;
    }

    const char current = static_cast<char>(raw);
    if (current == '\n' || current == '\r') {
      break;
    }

    headerLine[index++] = current;
  }
  headerLine[index] = '\0';
  headerFile.close();

  return strcmp(headerLine, EXPECTED_LOG_HEADER) == 0;
}

void SdManager::_writeLogHeader(File32& file) {
  for (size_t i = 0; i < LOG_COLUMN_COUNT; ++i) {
    file.print(LOG_COLUMNS[i]);
    file.print((i + 1 < LOG_COLUMN_COUNT) ? ',' : '\n');
  }
}

bool SdManager::_writeLogRow(File32& file, const LogRow& row) {
  const float lat = row.lat;
  const float lon = row.lon;
  char nowIso[kIso8601BufferLen];
  char txIso[kIso8601BufferLen];
  char ackIso[kIso8601BufferLen];

  _formatIso8601(_toEpochMs(row.nowMs), nowIso, sizeof(nowIso));
  _formatIso8601(_toEpochMs(row.txTime), txIso, sizeof(txIso));
  _formatIso8601(_toEpochMs(row.ackTime), ackIso, sizeof(ackIso));

  bool ok = true;

  auto mustWrite = [&](size_t count) {
    if (count == 0) {
      ok = false;
    }
  };

  // Print in the exact same order as LOG_COLUMNS
  mustWrite(file.print(nowIso)); mustWrite(file.print(','));
  mustWrite(file.print(row.nowMs)); mustWrite(file.print(','));
  mustWrite(file.print(txIso)); mustWrite(file.print(','));
  mustWrite(file.print(row.txTime)); mustWrite(file.print(','));
  mustWrite(file.print(ackIso)); mustWrite(file.print(','));
  mustWrite(file.print(row.ackTime)); mustWrite(file.print(','));
  mustWrite(file.print(row.rttMs)); mustWrite(file.print(','));
  mustWrite(file.print(row.runId)); mustWrite(file.print(','));
  mustWrite(file.print(row.role)); mustWrite(file.print(','));
  mustWrite(file.print(row.nodeId)); mustWrite(file.print(','));
  mustWrite(file.print(row.sf)); mustWrite(file.print(','));
  mustWrite(file.print(row.ackTimeoutMs)); mustWrite(file.print(','));
  mustWrite(file.print(lat, 6)); mustWrite(file.print(','));
  mustWrite(file.print(lon, 6)); mustWrite(file.print(','));
  mustWrite(file.print(row.rssi)); mustWrite(file.print(','));
  mustWrite(file.print(row.snr)); mustWrite(file.print(','));
  mustWrite(file.print(row.sessionId)); mustWrite(file.print(','));
  mustWrite(file.print(row.seqNum)); mustWrite(file.print(','));
  mustWrite(file.print(row.fragIndex)); mustWrite(file.print(','));
  mustWrite(file.print(row.fragLen)); mustWrite(file.print(','));
  mustWrite(file.print(row.packetType)); mustWrite(file.print(','));
  mustWrite(file.println(row.status));

  if (!ok) {
    return false;
  }

  file.flush();
  return file.isOpen();
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