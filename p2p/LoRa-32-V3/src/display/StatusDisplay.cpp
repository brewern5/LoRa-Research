#include "StatusDisplay.h"

// ── Static member definitions ────────────────────────────────────
// Heltec V3: I2C address 0x3C, SDA=17, SCL=18
SSD1306Wire StatusDisplay::_display(0x3c, SDA_OLED, SCL_OLED, GEOMETRY_128_64, I2C_ONE, 500000);

bool              StatusDisplay::_sdGood    = false;
StatusDisplay::LoRaState StatusDisplay::_loraState = StatusDisplay::LORA_FAIL;
uint32_t StatusDisplay::_txCount   = 0;
uint32_t StatusDisplay::_rxCount   = 0;
String StatusDisplay::_message   = "";
bool StatusDisplay::_dirty = false;

bool StatusDisplay::shouldDeferRedraw() {
  return _loraState == LORA_TRANSMITTING || _loraState == LORA_RECEIVING;
}

void StatusDisplay::requestRedraw(bool force) {
  if (!force && shouldDeferRedraw()) {
    _dirty = true;
    return;
  }
  _dirty = false;
  redraw();
}

// ── Lifecycle ────────────────────────────────────────────────────
void StatusDisplay::init() {
  // Heltec boards gate OLED power through Vext (active LOW).
  // If the symbol exists for this board variant, enable it before init.
#ifdef Vext
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(10);
#endif

  // Some variants expose an OLED reset pin; pulse it if available.
#ifdef RST_OLED
  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, LOW);
  delay(10);
  digitalWrite(RST_OLED, HIGH);
  delay(10);
#endif

  _display.init();
  _display.displayOn();
  _display.setFont(ArialMT_Plain_10);
  _display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Boot splash
  _display.clear();
  _display.setFont(ArialMT_Plain_16);
  _display.drawString(10, 10, "LoRa Node");
  _display.setFont(ArialMT_Plain_10);
  _display.drawString(10, 32, "Initializing...");
  _display.display();
  delay(1200);

  redraw();
}

// ── Public Endpoints ─────────────────────────────────────────────
void StatusDisplay::setSD(bool ok) {
  _sdGood = ok;
  requestRedraw();
}

void StatusDisplay::setLoRa(LoRaState state) {
  const bool wasBusy = (_loraState == LORA_TRANSMITTING || _loraState == LORA_RECEIVING);
  const bool willBeBusy = (state == LORA_TRANSMITTING || state == LORA_RECEIVING);

  if (_loraState == state && willBeBusy) {
    return;
  }

  bool leavingBusy = (wasBusy && !willBeBusy);
  _loraState = state;

  if (willBeBusy) {
    requestRedraw(true);
    return;
  }

  if (leavingBusy && _dirty) {
    requestRedraw(true);
    return;
  }

  requestRedraw();
}

void StatusDisplay::onPacketSent() {
  _txCount++;
  requestRedraw();
}

void StatusDisplay::onPacketReceived() {
  _rxCount++;
  requestRedraw();
}

void StatusDisplay::setMessage(const String& msg) {
  _message = msg;
  requestRedraw();
}

void StatusDisplay::clearMessage() {
  _message = "";
  requestRedraw();
}

void StatusDisplay::refresh() {
  requestRedraw(true);
}

// ── Private Redraw ───────────────────────────────────────────────
void StatusDisplay::redraw() {
  _display.clear();
  _display.setTextAlignment(TEXT_ALIGN_LEFT);

  // ── Title bar ─────────────────────────────────
  _display.setFont(ArialMT_Plain_10);
  _display.drawString(0, 0, "[ Node Status ]");
  _display.drawLine(0, 12, 127, 12);

  // ── SD Card row (y=16) ────────────────────────
  _display.drawString(0, 16, "SD:");
  if (_sdGood) {
    _display.drawString(24, 16, "GOOD");
    _display.fillRect(110, 16, 8, 8);   // solid square = OK
  } else {
    _display.drawString(24, 16, "FAIL");
    // Draw an X
    _display.drawLine(110, 16, 118, 24);
    _display.drawLine(118, 16, 110, 24);
  }

  // ── LoRa row (y=30) ───────────────────────────
  _display.drawString(0, 30, "LoRa:");
  switch (_loraState) {
    case LORA_OK_IDLE:
      _display.drawString(38, 30, "IDLE");
      _display.drawRect(110, 30, 8, 8);   // hollow square = idle
      break;
    case LORA_TRANSMITTING:
      _display.drawString(38, 30, "TX >>>");
      // Animate with a simple blink via millis parity
      if ((millis() / 300) % 2 == 0) _display.fillRect(110, 30, 8, 8);
      break;
    case LORA_RECEIVING:
      _display.drawString(38, 30, "<<< RX");
      if ((millis() / 300) % 2 == 0) _display.fillRect(110, 30, 8, 8);
      break;
    case LORA_FAIL:
      _display.drawString(38, 30, "FAIL");
      _display.drawLine(110, 30, 118, 38);
      _display.drawLine(118, 30, 110, 38);
      break;
  }

  // ── Packet counters (y=44) ────────────────────
  _display.drawString(0, 44, "TX:" + String(_txCount) + "  RX:" + String(_rxCount));

  // ── Message line (y=54) ───────────────────────
  if (_message.length() > 0) {
    _display.drawLine(0, 53, 127, 53);
    _display.drawString(0, 54, _message);
  }

  _display.display();
}
