/*
  Arduino GIGA R1 WiFi + GIGA Display Shield
  Retro serial terminal UI for video demos

  Required libraries / platform in Arduino IDE:
  - Board package: "Arduino Mbed OS GIGA Boards"
  - Library: "ArduinoGraphics"
    This is used by Arduino_H7_Video for text and drawing primitives.
  - Library: "Arduino_H7_Video"
    This ships with the GIGA board package and drives the GIGA Display Shield.

  Serial protocol:
  - The Mac can send plain text over USB Serial at 115200 baud.
  - An external device can also send plain text over hardware UART Serial1
    at 115200 baud.
  - Normal text is rendered into the on-screen terminal.
  - Send a newline ('\n') to commit a full line.
  - Commands must be sent as their own full line:
      /clear
      /divider
  - Partial text that does not start with '/' is also flushed automatically
    after a short idle time so streamed text still appears quickly.

  Example Mac test commands:
  1. Find the port:
     ls /dev/cu.usbmodem*

  2. Configure the port:
     stty -f /dev/cu.usbmodemXXXX 115200 raw -echo -echoe -echok

  3. Send text over USB:
     printf 'Hello from macOS\n' > /dev/cu.usbmodemXXXX
     printf '/divider\nSystem initializing...\nSensors online.\n' > /dev/cu.usbmodemXXXX
     printf '/clear\n' > /dev/cu.usbmodemXXXX

  Wiring for external UART input on the GIGA:
  - Use Serial1 on the GIGA.
  - UNO Q TX -> GIGA RX1
  - UNO Q RX -> GIGA TX1 (only needed if you want two-way traffic)
  - GND -> GND
*/

#include "Arduino_H7_Video.h"
#include "ArduinoGraphics.h"
#include <WiFi.h>
#include <SPI.h>
#include <string.h>
#include "giga_config.h"

Arduino_H7_Video Display(800, 480, GigaDisplayShield);

const char WIFI_SSID[] = CYBERDECK_WIFI_SSID;
const char WIFI_PASS[] = CYBERDECK_WIFI_PASS;
const char UNOQ_HOST[] = CYBERDECK_UNOQ_HOST;
const uint16_t UNOQ_PORT = CYBERDECK_UNOQ_PORT;

namespace Ui {
const uint32_t BLACK = 0x000000;
const uint32_t PANEL = 0x07111f;
const uint32_t PANEL_EDGE = 0x1d3f6d;
const uint32_t HEADER = 0x0a1b33;
const uint32_t HEADER_EDGE = 0x2f6fb8;
const uint32_t GREEN = 0xf2f7ff;
const uint32_t GREEN_SOFT = 0x8fc4ff;
const uint32_t GREEN_DIM = 0x5a95d6;
const uint32_t GREEN_FAINT = 0x18365f;
const uint32_t AMBER = 0x7fc0ff;
const uint32_t TEXT_USER = 0xf7fbff;
const uint32_t TEXT_RESPONSE = 0x4db8ff;
const uint32_t TEXT_OTHER = 0x7fa8d6;
const uint32_t TEXT_PREFIX = 0x7fa8d6;
const char PROMPT_PREFIX[] = "Prompt>> ";
const char RESPONSE_PREFIX[] = "QWEN>> ";
const char RESPONSE_SEPARATOR[] = "-----------------------------";

const int OUTER_MARGIN = 14;
const int HEADER_H = 0;
const int STATUS_H = 0;
const int PANEL_PAD_X = 14;
const int PANEL_PAD_Y = 12;
const int LINE_GAP = 3;

const uint32_t SERIAL_BAUD = 115200;
const unsigned long CURSOR_BLINK_MS = 530;
const unsigned long LIVE_BLINK_MS = 700;
const unsigned long PARTIAL_FLUSH_MS = 45;
const unsigned long WIFI_POLL_MS = 60;
const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long WIFI_STATUS_REFRESH_MS = 1000;
const unsigned long HTTP_TIMEOUT_MS = 1200;

const int MAX_ROWS = 28;
const int MAX_COLS = 96;
const int RX_BUFFER_LIMIT = 220;

// Wire a momentary push button between D2 and GND.
const pin_size_t DISPLAY_TOGGLE_PIN = D2;
const unsigned long BUTTON_DEBOUNCE_MS = 80;

// Default wiring expects one side of the button on D2 and the other on GND.
// Change to false if your button is wired from D2 to 3V3 instead.
const unsigned long MODEL_POLL_MS = 5000;
const uint32_t OLED_SPI_CLOCK_HZ = 8000000;
const pin_size_t OLED_CS_PIN = D4;
const pin_size_t OLED_DC_PIN = D5;
const pin_size_t OLED_RST_PIN = D6;
const uint8_t OLED_WIDTH = 64;
const uint8_t OLED_HEIGHT = 128;
const size_t OLED_BUFFER_SIZE = (OLED_WIDTH / 8) * OLED_HEIGHT;
}  // namespace Ui

enum OledRotation : uint16_t {
  OLED_ROTATE_0 = 0,
  OLED_ROTATE_90 = 90,
  OLED_ROTATE_180 = 180,
  OLED_ROTATE_270 = 270,
};

uint8_t oledFramebuffer[Ui::OLED_BUFFER_SIZE];

char terminalRows[Ui::MAX_ROWS][Ui::MAX_COLS + 1];
uint32_t terminalRowColors[Ui::MAX_ROWS];
bool rowDirty[Ui::MAX_ROWS];

String rxLine;

int terminalX = 0;
int terminalY = 0;
int terminalW = 0;
int terminalH = 0;
int textX = 0;
int textY = 0;
int textW = 0;
int textH = 0;
int charW = 0;
int charH = 0;
int linePitch = 0;
int visibleRows = 0;
int visibleCols = 0;

int cursorRow = 0;
int cursorCol = 0;
bool cursorOn = true;
bool liveOn = true;
bool fullFrameDirty = true;
bool headerDirty = true;
int lastCursorRow = 0;
int lastCursorCol = 0;
bool lastCursorWasVisible = false;

unsigned long lastCursorToggleMs = 0;
unsigned long lastLiveToggleMs = 0;
unsigned long lastSerialByteMs = 0;
unsigned long lastUsbHeartbeatMs = 0;
unsigned long lastWiFiPollMs = 0;
unsigned long lastWiFiRetryMs = 0;
unsigned long lastWiFiStatusRefreshMs = 0;
unsigned long lastModelPollMs = 0;
int wifiStatus = WL_IDLE_STATUS;
bool bridgeHealthy = false;
bool oledReady = false;
String wifiLabel = "WIFI INIT";
String activeModelPath = "";
String activeModelLabel = "WAITING FOR /MODEL";
bool displayEnabled = true;
bool buttonStableState = HIGH;
bool buttonPressedLatched = false;
bool lastButtonReading = HIGH;
bool screenBlackedOut = false;
unsigned long lastButtonEdgeMs = 0;
int buttonIdleLevel = HIGH;
int buttonActiveLevel = LOW;
bool incomingExpectingResponseBlock = false;
bool incomingResponseBlockIsLlm = false;

void renderDisplayOffScreen();
void applyDisplayState(bool enabled);
void refreshModelDisplay();
void runOledStartupTest();
void initOledDisplay();
void sendOledFramebuffer();
void setOledPanelEnabled(bool enabled);
void clearOledBuffer(uint8_t color);
void drawOledRect(int x, int y, int w, int h, uint8_t color, OledRotation rotation);
void fillOledRect(int x, int y, int w, int h, uint8_t color, OledRotation rotation);
void drawOledText5x7(int x, int y, const char* text, uint8_t color,
                     OledRotation rotation);
int rowVisibleLength(int row);
void drawRowSegment(int row, int startCol, const char* text, uint32_t color);
void drawTerminalRow(int row);

void writeOledCommand(uint8_t value) {
  digitalWrite(Ui::OLED_DC_PIN, LOW);
  digitalWrite(Ui::OLED_CS_PIN, LOW);
  SPI1.transfer(value);
  digitalWrite(Ui::OLED_CS_PIN, HIGH);
}

void writeOledData(uint8_t value) {
  digitalWrite(Ui::OLED_DC_PIN, HIGH);
  digitalWrite(Ui::OLED_CS_PIN, LOW);
  SPI1.transfer(value);
  digitalWrite(Ui::OLED_CS_PIN, HIGH);
}

void resetOledDisplay() {
  digitalWrite(Ui::OLED_RST_PIN, HIGH);
  delay(100);
  digitalWrite(Ui::OLED_RST_PIN, LOW);
  delay(100);
  digitalWrite(Ui::OLED_RST_PIN, HIGH);
  delay(100);
}

void initOledDisplay() {
  pinMode(Ui::OLED_CS_PIN, OUTPUT);
  pinMode(Ui::OLED_DC_PIN, OUTPUT);
  pinMode(Ui::OLED_RST_PIN, OUTPUT);
  digitalWrite(Ui::OLED_CS_PIN, HIGH);
  digitalWrite(Ui::OLED_DC_PIN, HIGH);
  digitalWrite(Ui::OLED_RST_PIN, HIGH);

  SPI1.begin();
  resetOledDisplay();

  SPI1.beginTransaction(SPISettings(Ui::OLED_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
  writeOledCommand(0xAE);
  writeOledCommand(0x00);
  writeOledCommand(0x10);
  writeOledCommand(0x20);
  writeOledCommand(0x00);
  writeOledCommand(0xFF);
  writeOledCommand(0xA6);
  writeOledCommand(0xA8);
  writeOledCommand(0x3F);
  writeOledCommand(0xD3);
  writeOledCommand(0x00);
  writeOledCommand(0xD5);
  writeOledCommand(0x80);
  writeOledCommand(0xD9);
  writeOledCommand(0x22);
  writeOledCommand(0xDA);
  writeOledCommand(0x12);
  writeOledCommand(0xDB);
  writeOledCommand(0x40);
  delay(200);
  writeOledCommand(0xAF);
  SPI1.endTransaction();
}

void sendOledFramebuffer() {
  SPI1.beginTransaction(SPISettings(Ui::OLED_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
  for (uint8_t page = 0; page < 8; ++page) {
    writeOledCommand(0xB0 + page);
    writeOledCommand(0x00);
    writeOledCommand(0x10);
    for (uint8_t column = 0; column < 128; ++column) {
      writeOledData(oledFramebuffer[(7 - page) + column * 8]);
    }
  }
  SPI1.endTransaction();
}

void setOledPanelEnabled(bool enabled) {
  if (!oledReady) {
    return;
  }

  SPI1.beginTransaction(SPISettings(Ui::OLED_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
  writeOledCommand(enabled ? 0xAF : 0xAE);
  SPI1.endTransaction();
}

void clearOledBuffer(uint8_t color) {
  memset(oledFramebuffer, color ? 0xFF : 0x00, sizeof(oledFramebuffer));
}

void setOledPixelRaw(uint8_t x, uint8_t y, uint8_t color) {
  if (x >= Ui::OLED_WIDTH || y >= Ui::OLED_HEIGHT) {
    return;
  }

  const size_t index =
      (x / 8) + static_cast<size_t>(y) * (Ui::OLED_WIDTH / 8);
  const uint8_t mask = 0x80 >> (x % 8);
  if (color) {
    oledFramebuffer[index] |= mask;
  } else {
    oledFramebuffer[index] &= ~mask;
  }
}

void setOledPixelRotated(uint8_t x, uint8_t y, uint8_t color,
                         OledRotation rotation) {
  if (x >= 128 || y >= 64) {
    return;
  }

  uint8_t mappedX = 0;
  uint8_t mappedY = 0;
  switch (rotation) {
    case OLED_ROTATE_0:
      mappedX = x;
      mappedY = y;
      break;
    case OLED_ROTATE_90:
      mappedX = Ui::OLED_WIDTH - y - 1;
      mappedY = x;
      break;
    case OLED_ROTATE_180:
      mappedX = Ui::OLED_WIDTH - x - 1;
      mappedY = Ui::OLED_HEIGHT - y - 1;
      break;
    case OLED_ROTATE_270:
      mappedX = y;
      mappedY = Ui::OLED_HEIGHT - x - 1;
      break;
  }

  setOledPixelRaw(mappedX, mappedY, color);
}

void drawOledLine(int x0, int y0, int x1, int y1, uint8_t color,
                  OledRotation rotation) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    setOledPixelRotated(static_cast<uint8_t>(x0), static_cast<uint8_t>(y0),
                        color, rotation);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void drawOledRect(int x, int y, int w, int h, uint8_t color,
                  OledRotation rotation) {
  drawOledLine(x, y, x + w - 1, y, color, rotation);
  drawOledLine(x, y + h - 1, x + w - 1, y + h - 1, color, rotation);
  drawOledLine(x, y, x, y + h - 1, color, rotation);
  drawOledLine(x + w - 1, y, x + w - 1, y + h - 1, color, rotation);
}

void fillOledRect(int x, int y, int w, int h, uint8_t color,
                  OledRotation rotation) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      setOledPixelRotated(static_cast<uint8_t>(xx), static_cast<uint8_t>(yy),
                          color, rotation);
    }
  }
}

bool lookupOledGlyph5x7(char c, uint8_t glyph[5]) {
  memset(glyph, 0, 5);
  switch (c) {
    case '0': glyph[0] = 0x3E; glyph[1] = 0x51; glyph[2] = 0x49; glyph[3] = 0x45; glyph[4] = 0x3E; return true;
    case '1': glyph[0] = 0x00; glyph[1] = 0x42; glyph[2] = 0x7F; glyph[3] = 0x40; glyph[4] = 0x00; return true;
    case '2': glyph[0] = 0x62; glyph[1] = 0x51; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x46; return true;
    case '3': glyph[0] = 0x22; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x36; return true;
    case '4': glyph[0] = 0x18; glyph[1] = 0x14; glyph[2] = 0x12; glyph[3] = 0x7F; glyph[4] = 0x10; return true;
    case '5': glyph[0] = 0x2F; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x31; return true;
    case '6': glyph[0] = 0x3E; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x32; return true;
    case '7': glyph[0] = 0x01; glyph[1] = 0x71; glyph[2] = 0x09; glyph[3] = 0x05; glyph[4] = 0x03; return true;
    case '8': glyph[0] = 0x36; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x36; return true;
    case '9': glyph[0] = 0x26; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x3E; return true;
    case '.': glyph[2] = 0x40; return true;
    case '-': glyph[0] = 0x08; glyph[1] = 0x08; glyph[2] = 0x08; glyph[3] = 0x08; glyph[4] = 0x08; return true;
    case '/': glyph[0] = 0x20; glyph[1] = 0x10; glyph[2] = 0x08; glyph[3] = 0x04; glyph[4] = 0x02; return true;
    case '_': glyph[4] = 0x40; return true;
    case ' ': return true;
    case 'A': glyph[0] = 0x7E; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x7E; return true;
    case 'B': glyph[0] = 0x7F; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x36; return true;
    case 'C': glyph[0] = 0x3E; glyph[1] = 0x41; glyph[2] = 0x41; glyph[3] = 0x41; glyph[4] = 0x22; return true;
    case 'D': glyph[0] = 0x7F; glyph[1] = 0x41; glyph[2] = 0x41; glyph[3] = 0x22; glyph[4] = 0x1C; return true;
    case 'E': glyph[0] = 0x7F; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x41; return true;
    case 'F': glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x01; return true;
    case 'G': glyph[0] = 0x3E; glyph[1] = 0x41; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x7A; return true;
    case 'H': glyph[0] = 0x7F; glyph[1] = 0x08; glyph[2] = 0x08; glyph[3] = 0x08; glyph[4] = 0x7F; return true;
    case 'I': glyph[0] = 0x00; glyph[1] = 0x41; glyph[2] = 0x7F; glyph[3] = 0x41; glyph[4] = 0x00; return true;
    case 'K': glyph[0] = 0x7F; glyph[1] = 0x08; glyph[2] = 0x14; glyph[3] = 0x22; glyph[4] = 0x41; return true;
    case 'L': glyph[0] = 0x7F; glyph[1] = 0x40; glyph[2] = 0x40; glyph[3] = 0x40; glyph[4] = 0x40; return true;
    case 'M': glyph[0] = 0x7F; glyph[1] = 0x02; glyph[2] = 0x0C; glyph[3] = 0x02; glyph[4] = 0x7F; return true;
    case 'N': glyph[0] = 0x7F; glyph[1] = 0x04; glyph[2] = 0x08; glyph[3] = 0x10; glyph[4] = 0x7F; return true;
    case 'O': glyph[0] = 0x3E; glyph[1] = 0x41; glyph[2] = 0x41; glyph[3] = 0x41; glyph[4] = 0x3E; return true;
    case 'P': glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x06; return true;
    case 'Q': glyph[0] = 0x3E; glyph[1] = 0x41; glyph[2] = 0x51; glyph[3] = 0x21; glyph[4] = 0x5E; return true;
    case 'R': glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x19; glyph[3] = 0x29; glyph[4] = 0x46; return true;
    case 'S': glyph[0] = 0x46; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x31; return true;
    case 'T': glyph[0] = 0x01; glyph[1] = 0x01; glyph[2] = 0x7F; glyph[3] = 0x01; glyph[4] = 0x01; return true;
    case 'U': glyph[0] = 0x3F; glyph[1] = 0x40; glyph[2] = 0x40; glyph[3] = 0x40; glyph[4] = 0x3F; return true;
    case 'V': glyph[0] = 0x1F; glyph[1] = 0x20; glyph[2] = 0x40; glyph[3] = 0x20; glyph[4] = 0x1F; return true;
    case 'W': glyph[0] = 0x7F; glyph[1] = 0x20; glyph[2] = 0x18; glyph[3] = 0x20; glyph[4] = 0x7F; return true;
    case 'X': glyph[0] = 0x63; glyph[1] = 0x14; glyph[2] = 0x08; glyph[3] = 0x14; glyph[4] = 0x63; return true;
    case 'Y': glyph[0] = 0x03; glyph[1] = 0x04; glyph[2] = 0x78; glyph[3] = 0x04; glyph[4] = 0x03; return true;
    default: return false;
  }
}

void drawOledChar5x7(int x, int y, char c, uint8_t color,
                     OledRotation rotation) {
  uint8_t glyph[5];
  lookupOledGlyph5x7(c, glyph);
  for (uint8_t col = 0; col < 5; ++col) {
    const uint8_t bits = glyph[col];
    for (uint8_t row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        setOledPixelRotated(x + col, y + row, color, rotation);
      }
    }
  }
}

void drawOledText5x7(int x, int y, const char* text, uint8_t color,
                     OledRotation rotation) {
  while (*text != '\0') {
    drawOledChar5x7(x, y, *text, color, rotation);
    x += 6;
    ++text;
  }
}

String basenameFromPath(const String& path) {
  const int slash = path.lastIndexOf('/');
  if (slash >= 0 && slash + 1 < static_cast<int>(path.length())) {
    return path.substring(slash + 1);
  }
  return path;
}

String trimModelLabel(const String& rawLabel) {
  String label = rawLabel;
  label.trim();
  if (label.isEmpty()) {
    return "MODEL UNKNOWN";
  }
  return label;
}

void refreshModelDisplay() {
  if (!oledReady) {
    return;
  }

  const String title = "RUNNING MODEL:";
  const String modelLabel = trimModelLabel(activeModelLabel);
  const int maxCharsPerLine = 16;

  String line1 = modelLabel;
  String line2 = "";
  String line3 = "";

  if (line1.length() > maxCharsPerLine) {
    int split = line1.lastIndexOf('-', maxCharsPerLine);
    if (split <= 0) {
      split = line1.lastIndexOf('_', maxCharsPerLine);
    }
    if (split <= 0) {
      split = line1.lastIndexOf('.', maxCharsPerLine);
    }
    if (split <= 0) {
      split = maxCharsPerLine;
    }
    line2 = line1.substring(split);
    line1 = line1.substring(0, split);
    line2.trim();
  }

  if (line2.length() > maxCharsPerLine) {
    int split = line2.lastIndexOf('-', maxCharsPerLine);
    if (split <= 0) {
      split = line2.lastIndexOf('_', maxCharsPerLine);
    }
    if (split <= 0) {
      split = line2.lastIndexOf('.', maxCharsPerLine);
    }
    if (split <= 0) {
      split = maxCharsPerLine;
    }
    line3 = line2.substring(split);
    line2 = line2.substring(0, split);
    line3.trim();
  }

  if (line3.length() > maxCharsPerLine) {
    line3 = line3.substring(0, maxCharsPerLine - 1) + ".";
  }

  line1.toUpperCase();
  line2.toUpperCase();
  line3.toUpperCase();

  clearOledBuffer(0);
  drawOledRect(0, 0, 128, 64, 1, OLED_ROTATE_270);
  drawOledRect(2, 2, 124, 60, 1, OLED_ROTATE_270);
  drawOledText5x7(8, 6, title.c_str(), 1, OLED_ROTATE_270);
  drawOledText5x7(8, 22, line1.c_str(), 1, OLED_ROTATE_270);
  if (!line2.isEmpty()) {
    drawOledText5x7(8, 34, line2.c_str(), 1, OLED_ROTATE_270);
  }
  if (!line3.isEmpty()) {
    drawOledText5x7(8, 46, line3.c_str(), 1, OLED_ROTATE_270);
  }
  sendOledFramebuffer();
}

void runOledStartupTest() {
  if (!oledReady) {
    return;
  }

  clearOledBuffer(0);
  drawOledRect(0, 0, 128, 64, 1, OLED_ROTATE_270);
  drawOledRect(2, 2, 124, 60, 1, OLED_ROTATE_270);
  drawOledText5x7(22, 16, "OLED OK", 1, OLED_ROTATE_270);
  drawOledText5x7(10, 30, "WAVESHARE SPI1", 1, OLED_ROTATE_270);
  fillOledRect(10, 44, 108, 10, 1, OLED_ROTATE_270);
  sendOledFramebuffer();
  delay(1200);
}

void setActiveModelLabel(const String& nextModelPath) {
  const String nextPath = trimModelLabel(nextModelPath);
  const String nextLabel = basenameFromPath(nextPath);
  if (activeModelPath == nextPath && activeModelLabel == nextLabel) {
    return;
  }

  activeModelPath = nextPath;
  activeModelLabel = nextLabel;
  refreshModelDisplay();
}

void fetchModelLabelIfNeeded(bool force) {
  if (!oledReady || WiFi.status() != WL_CONNECTED || !bridgeHealthy) {
    return;
  }

  const unsigned long now = millis();
  if (!force && now - lastModelPollMs < Ui::MODEL_POLL_MS) {
    return;
  }
  lastModelPollMs = now;

  String body;
  if (!httpGetText("/model", body)) {
    return;
  }

  setActiveModelLabel(body);
}

[[noreturn]] void fatalBlink() {
  pinMode(LEDR, OUTPUT);
  while (true) {
    digitalWrite(LEDR, LOW);
    delay(160);
    digitalWrite(LEDR, HIGH);
    delay(160);
  }
}

void clearRowBuffer(int row) {
  for (int col = 0; col < Ui::MAX_COLS; ++col) {
    terminalRows[row][col] = ' ';
  }
  terminalRows[row][Ui::MAX_COLS] = '\0';
  terminalRowColors[row] = Ui::TEXT_OTHER;
}

void markAllRowsDirty() {
  for (int row = 0; row < visibleRows; ++row) {
    rowDirty[row] = true;
  }
}

void drawHeader();
void renderTerminal();

void setupLayout() {
  Display.textFont(Font_5x7);
  Display.textSize(2);

  charW = Display.textFontWidth();
  charH = Display.textFontHeight();
  linePitch = charH + Ui::LINE_GAP;

  terminalX = Ui::OUTER_MARGIN;
  terminalY = Ui::OUTER_MARGIN;
  terminalW = Display.width() - (Ui::OUTER_MARGIN * 2);
  terminalH = Display.height() - terminalY - Ui::OUTER_MARGIN;

  textX = terminalX + Ui::PANEL_PAD_X;
  textY = terminalY + Ui::PANEL_PAD_Y;
  textW = terminalW - (Ui::PANEL_PAD_X * 2);
  textH = terminalH - (Ui::PANEL_PAD_Y * 2);

  visibleCols = textW / charW;
  visibleRows = textH / linePitch;

  if (visibleCols > Ui::MAX_COLS) {
    visibleCols = Ui::MAX_COLS;
  }
  if (visibleRows > Ui::MAX_ROWS) {
    visibleRows = Ui::MAX_ROWS;
  }
}

int rowPixelY(int row) {
  return textY + (row * linePitch);
}

void clearTextRowPixels(int row) {
  const int y = rowPixelY(row);
  const uint32_t band = (row % 2 == 0) ? Ui::BLACK : Ui::PANEL;

  Display.noStroke();
  Display.fill(band);
  Display.rect(textX - 2, y - 1, textW + 4, linePitch);

  Display.stroke(Ui::GREEN_FAINT);
  Display.line(textX - 2, y + linePitch - 2, textX + textW + 1, y + linePitch - 2);
}

void drawCursor() {
  if (!cursorOn) {
    return;
  }

  const int x = textX + (cursorCol * charW);
  const int y = rowPixelY(cursorRow);
  const int cursorWidth = charW - 2;
  const int cursorHeight = charH + 1;

  Display.noStroke();
  Display.fill(Ui::GREEN);
  Display.rect(x, y + charH - 2, cursorWidth, 3);

  Display.fill(Ui::GREEN_SOFT);
  Display.rect(x, y, 2, cursorHeight);
}

void redrawCharCell(int row, int col) {
  if (row < 0 || row >= visibleRows || col < 0 || col >= visibleCols) {
    return;
  }

  clearTextRowPixels(row);
  drawTerminalRow(row);
}

void drawStatusBar() {
}

void drawHeader() {
}

void drawFrame() {
  Display.background(Ui::BLACK);
  Display.clear();

  drawHeader();

  Display.noStroke();
  Display.fill(Ui::PANEL_EDGE);
  Display.rect(terminalX - 2, terminalY - 2, terminalW + 4, terminalH + 4);

  Display.fill(Ui::PANEL);
  Display.rect(terminalX, terminalY, terminalW, terminalH);

  for (int row = 0; row < visibleRows; ++row) {
    clearTextRowPixels(row);
  }

  drawStatusBar();
}

void renderDisplayOffScreen() {
  Display.beginDraw();
  Display.background(Ui::BLACK);
  Display.clear();
  Display.noStroke();
  Display.fill(Ui::BLACK);
  Display.rect(0, 0, Display.width(), Display.height());
  Display.endDraw();
  screenBlackedOut = true;
}

void resetTerminalBuffer() {
  for (int row = 0; row < Ui::MAX_ROWS; ++row) {
    clearRowBuffer(row);
    rowDirty[row] = false;
  }

  cursorRow = 0;
  cursorCol = 0;
  cursorOn = true;
  lastCursorRow = 0;
  lastCursorCol = 0;
  lastCursorWasVisible = false;
  markAllRowsDirty();
}

void scrollUp() {
  for (int row = 1; row < visibleRows; ++row) {
    memcpy(terminalRows[row - 1], terminalRows[row], Ui::MAX_COLS + 1);
    terminalRowColors[row - 1] = terminalRowColors[row];
    rowDirty[row - 1] = true;
  }

  clearRowBuffer(visibleRows - 1);
  rowDirty[visibleRows - 1] = true;
}

void newLine() {
  cursorCol = 0;
  ++cursorRow;

  if (cursorRow >= visibleRows) {
    scrollUp();
    cursorRow = visibleRows - 1;
  }
}

void putChar(char c, uint32_t color) {
  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    rowDirty[cursorRow] = true;
    newLine();
    return;
  }

  if (c == '\t') {
    const int spaces = 4 - (cursorCol % 4);
    for (int i = 0; i < spaces; ++i) {
      putChar(' ', color);
    }
    return;
  }

  if (c < 32 || c > 126) {
    c = '.';
  }

  if (cursorCol >= visibleCols) {
    newLine();
  }

  terminalRowColors[cursorRow] = color;
  terminalRows[cursorRow][cursorCol] = c;
  rowDirty[cursorRow] = true;
  ++cursorCol;

  if (cursorCol >= visibleCols) {
    newLine();
  }
}

void printTextToTerminal(const String& text, bool appendNewline, uint32_t color) {
  for (size_t i = 0; i < text.length(); ++i) {
    putChar(text[i], color);
  }

  if (appendNewline) {
    putChar('\n', color);
  }
}

void printDivider() {
  if (cursorCol != 0) {
    putChar('\n', Ui::TEXT_OTHER);
  }

  String divider;
  divider.reserve(visibleCols);

  for (int col = 0; col < visibleCols; ++col) {
    divider += (col % 6 == 0) ? '+' : '-';
  }

  printTextToTerminal(divider, true, Ui::TEXT_OTHER);
}

void clearTerminal() {
  resetTerminalBuffer();
  fullFrameDirty = true;
}

void processIncomingLine(String line) {
  if (line == "/clear") {
    clearTerminal();
    incomingExpectingResponseBlock = false;
    incomingResponseBlockIsLlm = false;
    return;
  }

  if (line == "/divider") {
    printDivider();
    incomingExpectingResponseBlock = false;
    incomingResponseBlockIsLlm = false;
    return;
  }

  uint32_t color = Ui::TEXT_OTHER;

  if (line.startsWith(Ui::PROMPT_PREFIX)) {
    color = Ui::TEXT_USER;
    incomingExpectingResponseBlock = true;
    incomingResponseBlockIsLlm =
        !line.startsWith(String(Ui::PROMPT_PREFIX) + "/");
  } else if (line == Ui::RESPONSE_SEPARATOR) {
    color = Ui::TEXT_PREFIX;
  } else if (line.startsWith(Ui::RESPONSE_PREFIX)) {
    color = Ui::TEXT_RESPONSE;
  } else if (line.length() == 0) {
    incomingExpectingResponseBlock = false;
    incomingResponseBlockIsLlm = false;
  } else if (incomingExpectingResponseBlock && incomingResponseBlockIsLlm) {
    color = Ui::TEXT_RESPONSE;
  }

  printTextToTerminal(line, true, color);
}

void processIncomingPayload(const String& payload) {
  if (payload.isEmpty()) {
    return;
  }

  String line;
  for (size_t i = 0; i < payload.length(); ++i) {
    const char c = payload[i];
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      processIncomingLine(line);
      line = "";
      continue;
    }
    line += c;
  }

  if (!line.isEmpty()) {
    processIncomingLine(line);
  }
}

bool hasDirtyRows() {
  for (int row = 0; row < visibleRows; ++row) {
    if (rowDirty[row]) {
      return true;
    }
  }
  return false;
}

int rowVisibleLength(int row) {
  int end = visibleCols;
  if (end > Ui::MAX_COLS) {
    end = Ui::MAX_COLS;
  }

  while (end > 0 && terminalRows[row][end - 1] == ' ') {
    --end;
  }
  return end;
}

void drawRowSegment(int row, int startCol, const char* text, uint32_t color) {
  if (text == nullptr || text[0] == '\0') {
    return;
  }

  Display.stroke(color);
  Display.text(text, textX + (startCol * charW), rowPixelY(row));
}

void drawTerminalRow(int row) {
  const int visibleLen = rowVisibleLength(row);
  if (visibleLen <= 0) {
    return;
  }

  char rowText[Ui::MAX_COLS + 1];
  memcpy(rowText, terminalRows[row], visibleLen);
  rowText[visibleLen] = '\0';

  const size_t promptPrefixLen = strlen(Ui::PROMPT_PREFIX);
  const size_t responsePrefixLen = strlen(Ui::RESPONSE_PREFIX);

  if (strncmp(rowText, Ui::PROMPT_PREFIX, promptPrefixLen) == 0) {
    drawRowSegment(row, 0, Ui::PROMPT_PREFIX, Ui::TEXT_PREFIX);
    drawRowSegment(row, static_cast<int>(promptPrefixLen),
                   rowText + promptPrefixLen, Ui::TEXT_USER);
    return;
  }

  if (strcmp(rowText, Ui::RESPONSE_SEPARATOR) == 0) {
    drawRowSegment(row, 0, rowText, Ui::TEXT_PREFIX);
    return;
  }

  if (strncmp(rowText, Ui::RESPONSE_PREFIX, responsePrefixLen) == 0) {
    drawRowSegment(row, 0, Ui::RESPONSE_PREFIX, Ui::TEXT_PREFIX);
    drawRowSegment(row, static_cast<int>(responsePrefixLen),
                   rowText + responsePrefixLen, Ui::TEXT_RESPONSE);
    return;
  }

  drawRowSegment(row, 0, rowText, terminalRowColors[row]);
}

void renderDirtyRows() {
  for (int row = 0; row < visibleRows; ++row) {
    if (!rowDirty[row]) {
      continue;
    }

    clearTextRowPixels(row);
    drawTerminalRow(row);
    rowDirty[row] = false;
  }
}

void updateCursorVisual() {
  if (lastCursorWasVisible) {
    redrawCharCell(lastCursorRow, lastCursorCol);
  }

  if (cursorOn) {
    drawCursor();
  }

  lastCursorRow = cursorRow;
  lastCursorCol = cursorCol;
  lastCursorWasVisible = cursorOn;
}

void renderTerminal() {
  if (!displayEnabled) {
    if (!screenBlackedOut) {
      renderDisplayOffScreen();
    }
    return;
  }

  Display.beginDraw();

  if (fullFrameDirty) {
    drawFrame();
    markAllRowsDirty();
    fullFrameDirty = false;
  }

  if (headerDirty) {
    drawHeader();
    headerDirty = false;
  }

  if (hasDirtyRows()) {
    renderDirtyRows();
  }

  updateCursorVisual();

  Display.endDraw();
  screenBlackedOut = false;
}

void bootLine(const __FlashStringHelper* text, unsigned long pauseMs) {
  printTextToTerminal(String(text), true, Ui::TEXT_OTHER);
  renderTerminal();
  delay(pauseMs);
}

void runBootSequence() {
  clearTerminal();
  renderTerminal();
}

void readSerialInput() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    lastSerialByteMs = millis();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      processIncomingLine(rxLine);
      rxLine = "";
      continue;
    }

    if (rxLine.length() < Ui::RX_BUFFER_LIMIT) {
      rxLine += c;
    } else {
      printTextToTerminal(rxLine, false, Ui::TEXT_OTHER);
      rxLine = "";
      rxLine += c;
    }
  }

  while (Serial1.available() > 0) {
    const char c = static_cast<char>(Serial1.read());
    lastSerialByteMs = millis();

    // Mirror external UART traffic to USB for debugging.
    Serial.write(static_cast<uint8_t>(c));

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      processIncomingLine(rxLine);
      rxLine = "";
      continue;
    }

    if (rxLine.length() < Ui::RX_BUFFER_LIMIT) {
      rxLine += c;
    } else {
      printTextToTerminal(rxLine, false, Ui::TEXT_OTHER);
      rxLine = "";
      rxLine += c;
    }
  }

  if (!rxLine.isEmpty() && !rxLine.startsWith("/") &&
      millis() - lastSerialByteMs > Ui::PARTIAL_FLUSH_MS) {
    printTextToTerminal(rxLine, false, Ui::TEXT_OTHER);
    rxLine = "";
  }
}

void updateBlinkState() {
  const unsigned long now = millis();

  if (now - lastCursorToggleMs >= Ui::CURSOR_BLINK_MS) {
    lastCursorToggleMs = now;
    cursorOn = !cursorOn;
    fullFrameDirty = true;
  }
}

void applyDisplayState(bool enabled) {
  if (displayEnabled == enabled) {
    return;
  }

  displayEnabled = enabled;
  cursorOn = enabled;
  setOledPanelEnabled(enabled);

  if (!enabled) {
    digitalWrite(LED_BUILTIN, LOW);
    renderDisplayOffScreen();
    Serial.println("[GIGA DISPLAY] off");
    return;
  }

  digitalWrite(LED_BUILTIN, HIGH);
  fullFrameDirty = true;
  headerDirty = true;
  markAllRowsDirty();
  lastCursorWasVisible = false;
  refreshModelDisplay();
  Serial.println("[GIGA DISPLAY] on");
}

void handleDisplayToggleButton() {
  const unsigned long now = millis();
  const bool reading = digitalRead(Ui::DISPLAY_TOGGLE_PIN);

  if (reading != lastButtonReading) {
    lastButtonEdgeMs = now;
    lastButtonReading = reading;
  }

  if (now - lastButtonEdgeMs < Ui::BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (reading != buttonStableState) {
    buttonStableState = reading;
  }

  if (buttonStableState != buttonIdleLevel) {
    if (!buttonPressedLatched) {
      buttonPressedLatched = true;
      Serial.print("[GIGA BUTTON] toggle on pin D");
      Serial.println(static_cast<int>(Ui::DISPLAY_TOGGLE_PIN));
      applyDisplayState(!displayEnabled);
    }
    return;
  }

  buttonPressedLatched = false;
}

void emitUsbHeartbeat() {
  const unsigned long now = millis();
  if (now - lastUsbHeartbeatMs >= 10000UL) {
    lastUsbHeartbeatMs = now;
    Serial.println("[GIGA USB OK] heartbeat");
    Serial.print("[GIGA WIFI STATE] ");
    Serial.println(wifiLabel);
  }
}

void setWiFiLabel(const String& nextLabel, bool nextBridgeHealthy) {
  if (wifiLabel != nextLabel || bridgeHealthy != nextBridgeHealthy) {
    wifiLabel = nextLabel;
    bridgeHealthy = nextBridgeHealthy;
    headerDirty = true;
    Serial.print("[GIGA WIFI] ");
    Serial.println(wifiLabel);
  }
}

const char* wifiEncryptionLabel(uint8_t enc) {
  switch (enc) {
    case ENC_TYPE_NONE:
      return "OPEN";
    case ENC_TYPE_WEP:
      return "WEP";
    case ENC_TYPE_TKIP:
      return "WPA";
    case ENC_TYPE_CCMP:
      return "WPA2";
    case ENC_TYPE_AUTO:
      return "AUTO";
    default:
      return "UNKNOWN";
  }
}

void logWiFiScan() {
  const int count = WiFi.scanNetworks();
  Serial.print("[GIGA WIFI SCAN] networks=");
  Serial.println(count);

  bool foundTarget = false;
  for (int i = 0; i < count; ++i) {
    const char* ssid = WiFi.SSID(i);
    if (ssid == nullptr) {
      continue;
    }

    if (String(ssid) == WIFI_SSID) {
      foundTarget = true;
      Serial.print("[GIGA WIFI TARGET] ssid=");
      Serial.print(ssid);
      Serial.print(" rssi=");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" enc=");
      Serial.println(wifiEncryptionLabel(WiFi.encryptionType(i)));
    }
  }

  if (!foundTarget) {
    Serial.print("[GIGA WIFI TARGET] ssid=");
    Serial.print(WIFI_SSID);
    Serial.println(" not visible");
  }
}

void refreshWiFiState() {
  const int nextStatus = WiFi.status();
  wifiStatus = nextStatus;

  if (nextStatus == WL_CONNECTED) {
    String label = "WIFI ";
    label += WiFi.localIP().toString();
    label += bridgeHealthy ? "  BRIDGE OK" : "  BRIDGE WAIT";
    setWiFiLabel(label, bridgeHealthy);
    return;
  }

  String label = "WIFI ";
  switch (nextStatus) {
    case WL_NO_MODULE:
      label += "NO MODULE";
      break;
    case WL_CONNECT_FAILED:
      label += "CONNECT FAIL";
      break;
    case WL_CONNECTION_LOST:
      label += "LINK LOST";
      break;
    case WL_DISCONNECTED:
      label += "DISCONNECTED";
      break;
    case WL_IDLE_STATUS:
    default:
      label += "JOINING";
      break;
  }
  setWiFiLabel(label, false);
}

bool connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  const unsigned long now = millis();
  if (now - lastWiFiRetryMs < Ui::WIFI_RETRY_MS) {
    return false;
  }

  lastWiFiRetryMs = now;
  logWiFiScan();
  WiFi.disconnect();
  WiFi.setTimeout(20000);
  wifiStatus = WiFi.begin(WIFI_SSID, WIFI_PASS, ENC_TYPE_CCMP);
  if (wifiStatus != WL_CONNECTED) {
    wifiStatus = WiFi.begin(WIFI_SSID, WIFI_PASS, ENC_TYPE_AUTO);
  }
  refreshWiFiState();
  return false;
}

bool httpGetText(const char* path, String& body) {
  WiFiClient client;
  body = "";

  if (!client.connect(UNOQ_HOST, UNOQ_PORT)) {
    return false;
  }

  client.setTimeout(Ui::HTTP_TIMEOUT_MS);
  client.print("GET ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(UNOQ_HOST);
  client.println("Connection: close");
  client.println();

  String response;
  unsigned long started = millis();
  while (millis() - started < Ui::HTTP_TIMEOUT_MS) {
    while (client.available() > 0) {
      response += static_cast<char>(client.read());
      started = millis();
    }

    if (!client.connected()) {
      break;
    }
    delay(2);
  }
  client.stop();

  const int headerEnd = response.indexOf("\r\n\r\n");
  if (headerEnd < 0) {
    return false;
  }

  body = response.substring(headerEnd + 4);
  return true;
}

void checkBridgeHealth() {
  String body;
  bridgeHealthy = httpGetText("/health", body) && body == "OK";
  refreshWiFiState();
  if (bridgeHealthy) {
    fetchModelLabelIfNeeded(true);
  }
}

void pollBridge() {
  const unsigned long now = millis();
  if (now - lastWiFiPollMs < Ui::WIFI_POLL_MS) {
    return;
  }
  lastWiFiPollMs = now;

  if (WiFi.status() != WL_CONNECTED) {
    bridgeHealthy = false;
    connectWiFiIfNeeded();
    refreshWiFiState();
    return;
  }

  String payload;
  if (!httpGetText("/latest", payload)) {
    bridgeHealthy = false;
    refreshWiFiState();
    return;
  }

  bridgeHealthy = true;
  refreshWiFiState();
  fetchModelLabelIfNeeded(false);

  if (!payload.isEmpty()) {
    Serial.print("[GIGA WIFI RX] ");
    Serial.println(payload);
    processIncomingPayload(payload);
  }
}

void setup() {
  Serial.begin(Ui::SERIAL_BAUD);
  Serial1.begin(Ui::SERIAL_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(
      Ui::DISPLAY_TOGGLE_PIN,
      INPUT_PULLUP);
  delay(30);
  buttonIdleLevel = HIGH;
  buttonActiveLevel = LOW;
  buttonStableState = buttonIdleLevel;
  buttonPressedLatched = false;
  lastButtonReading = buttonIdleLevel;
  delay(250);

  if (Display.begin() != 0) {
    fatalBlink();
  }

  initOledDisplay();
  oledReady = true;
  activeModelLabel = "BOOTING";
  runOledStartupTest();
  refreshModelDisplay();

  setupLayout();
  resetTerminalBuffer();
  runBootSequence();

  if (WiFi.status() != WL_NO_MODULE && WiFi.status() != WL_NO_SHIELD) {
    logWiFiScan();
    WiFi.setTimeout(20000);
    wifiStatus = WiFi.begin(WIFI_SSID, WIFI_PASS, ENC_TYPE_CCMP);
    if (wifiStatus != WL_CONNECTED) {
      wifiStatus = WiFi.begin(WIFI_SSID, WIFI_PASS, ENC_TYPE_AUTO);
    }
    delay(600);
  }
  refreshWiFiState();
  checkBridgeHealth();
  renderTerminal();
}

void loop() {
  readSerialInput();
  handleDisplayToggleButton();
  updateBlinkState();
  emitUsbHeartbeat();
  pollBridge();
  if (millis() - lastWiFiStatusRefreshMs >= Ui::WIFI_STATUS_REFRESH_MS) {
    lastWiFiStatusRefreshMs = millis();
    refreshWiFiState();
  }

  if (fullFrameDirty || headerDirty || hasDirtyRows()) {
    renderTerminal();
  }
}
