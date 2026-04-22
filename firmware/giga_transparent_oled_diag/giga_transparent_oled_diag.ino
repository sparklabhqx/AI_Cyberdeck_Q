#include <SPI.h>
#include <stdint.h>
#include <string.h>

namespace Pins {
const pin_size_t OLED_CS = D4;
const pin_size_t OLED_DC = D5;
const pin_size_t OLED_RST = D6;
}

namespace Oled {
const uint8_t WIDTH = 64;
const uint8_t HEIGHT = 128;
const size_t BUFFER_SIZE = (WIDTH / 8) * HEIGHT;
const uint32_t SPI_CLOCK_HZ = 8000000;
const uint8_t COLOR_BLACK = 0;
const uint8_t COLOR_WHITE = 1;
}

enum Rotation : uint16_t {
  ROTATE_0 = 0,
  ROTATE_90 = 90,
  ROTATE_180 = 180,
  ROTATE_270 = 270,
};

uint8_t framebuffer[Oled::BUFFER_SIZE];

void writeCommand(uint8_t value) {
  digitalWrite(Pins::OLED_DC, LOW);
  digitalWrite(Pins::OLED_CS, LOW);
  SPI1.transfer(value);
  digitalWrite(Pins::OLED_CS, HIGH);
}

void writeData(uint8_t value) {
  digitalWrite(Pins::OLED_DC, HIGH);
  digitalWrite(Pins::OLED_CS, LOW);
  SPI1.transfer(value);
  digitalWrite(Pins::OLED_CS, HIGH);
}

void resetDisplay() {
  digitalWrite(Pins::OLED_RST, HIGH);
  delay(100);
  digitalWrite(Pins::OLED_RST, LOW);
  delay(100);
  digitalWrite(Pins::OLED_RST, HIGH);
  delay(100);
}

void initDisplay() {
  resetDisplay();

  SPI1.beginTransaction(SPISettings(Oled::SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));

  writeCommand(0xAE);
  writeCommand(0x00);
  writeCommand(0x10);
  writeCommand(0x20);
  writeCommand(0x00);
  writeCommand(0xFF);
  writeCommand(0xA6);
  writeCommand(0xA8);
  writeCommand(0x3F);
  writeCommand(0xD3);
  writeCommand(0x00);
  writeCommand(0xD5);
  writeCommand(0x80);
  writeCommand(0xD9);
  writeCommand(0x22);
  writeCommand(0xDA);
  writeCommand(0x12);
  writeCommand(0xDB);
  writeCommand(0x40);
  delay(200);
  writeCommand(0xAF);

  SPI1.endTransaction();
}

void sendFramebuffer() {
  SPI1.beginTransaction(SPISettings(Oled::SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
  for (uint8_t page = 0; page < 8; ++page) {
    writeCommand(0xB0 + page);
    writeCommand(0x00);
    writeCommand(0x10);
    for (uint8_t column = 0; column < 128; ++column) {
      const uint8_t value = framebuffer[(7 - page) + column * 8];
      writeData(value);
    }
  }
  SPI1.endTransaction();
}

void clearBuffer(uint8_t color) {
  memset(framebuffer, color ? 0xFF : 0x00, sizeof(framebuffer));
}

void setPixelRaw(uint8_t x, uint8_t y, uint8_t color) {
  if (x >= Oled::WIDTH || y >= Oled::HEIGHT) {
    return;
  }

  const size_t index = (x / 8) + static_cast<size_t>(y) * (Oled::WIDTH / 8);
  const uint8_t mask = 0x80 >> (x % 8);
  if (color == Oled::COLOR_WHITE) {
    framebuffer[index] |= mask;
  } else {
    framebuffer[index] &= ~mask;
  }
}

void setPixelRotated(uint8_t x, uint8_t y, uint8_t color, Rotation rotation) {
  if (x >= 128 || y >= 64) {
    return;
  }

  uint8_t mappedX = 0;
  uint8_t mappedY = 0;

  switch (rotation) {
    case ROTATE_0:
      mappedX = x;
      mappedY = y;
      break;
    case ROTATE_90:
      mappedX = Oled::WIDTH - y - 1;
      mappedY = x;
      break;
    case ROTATE_180:
      mappedX = Oled::WIDTH - x - 1;
      mappedY = Oled::HEIGHT - y - 1;
      break;
    case ROTATE_270:
      mappedX = y;
      mappedY = Oled::HEIGHT - x - 1;
      break;
  }

  setPixelRaw(mappedX, mappedY, color);
}

void drawLine(int x0, int y0, int x1, int y1, uint8_t color, Rotation rotation) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    setPixelRotated(static_cast<uint8_t>(x0), static_cast<uint8_t>(y0), color, rotation);
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

void drawRect(int x, int y, int w, int h, uint8_t color, Rotation rotation) {
  drawLine(x, y, x + w - 1, y, color, rotation);
  drawLine(x, y + h - 1, x + w - 1, y + h - 1, color, rotation);
  drawLine(x, y, x, y + h - 1, color, rotation);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, color, rotation);
}

void fillRect(int x, int y, int w, int h, uint8_t color, Rotation rotation) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      setPixelRotated(static_cast<uint8_t>(xx), static_cast<uint8_t>(yy), color, rotation);
    }
  }
}

bool lookupGlyph5x7(char c, uint8_t glyph[5]) {
  memset(glyph, 0, 5);
  switch (c) {
    case '0':
      glyph[0] = 0x3E; glyph[1] = 0x51; glyph[2] = 0x49; glyph[3] = 0x45; glyph[4] = 0x3E; return true;
    case '1':
      glyph[0] = 0x00; glyph[1] = 0x42; glyph[2] = 0x7F; glyph[3] = 0x40; glyph[4] = 0x00; return true;
    case '2':
      glyph[0] = 0x62; glyph[1] = 0x51; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x46; return true;
    case '7':
      glyph[0] = 0x01; glyph[1] = 0x71; glyph[2] = 0x09; glyph[3] = 0x05; glyph[4] = 0x03; return true;
    case '9':
      glyph[0] = 0x26; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x3E; return true;
    case 'A':
      glyph[0] = 0x7E; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x7E; return true;
    case 'D':
      glyph[0] = 0x7F; glyph[1] = 0x41; glyph[2] = 0x41; glyph[3] = 0x22; glyph[4] = 0x1C; return true;
    case 'E':
      glyph[0] = 0x7F; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x41; return true;
    case 'F':
      glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x01; return true;
    case 'H':
      glyph[0] = 0x7F; glyph[1] = 0x08; glyph[2] = 0x08; glyph[3] = 0x08; glyph[4] = 0x7F; return true;
    case 'I':
      glyph[0] = 0x00; glyph[1] = 0x41; glyph[2] = 0x7F; glyph[3] = 0x41; glyph[4] = 0x00; return true;
    case 'L':
      glyph[0] = 0x7F; glyph[1] = 0x40; glyph[2] = 0x40; glyph[3] = 0x40; glyph[4] = 0x40; return true;
    case 'M':
      glyph[0] = 0x7F; glyph[1] = 0x02; glyph[2] = 0x0C; glyph[3] = 0x02; glyph[4] = 0x7F; return true;
    case 'O':
      glyph[0] = 0x3E; glyph[1] = 0x41; glyph[2] = 0x41; glyph[3] = 0x41; glyph[4] = 0x3E; return true;
    case 'P':
      glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x09; glyph[3] = 0x09; glyph[4] = 0x06; return true;
    case 'R':
      glyph[0] = 0x7F; glyph[1] = 0x09; glyph[2] = 0x19; glyph[3] = 0x29; glyph[4] = 0x46; return true;
    case 'S':
      glyph[0] = 0x46; glyph[1] = 0x49; glyph[2] = 0x49; glyph[3] = 0x49; glyph[4] = 0x31; return true;
    case 'T':
      glyph[0] = 0x01; glyph[1] = 0x01; glyph[2] = 0x7F; glyph[3] = 0x01; glyph[4] = 0x01; return true;
    case 'V':
      glyph[0] = 0x1F; glyph[1] = 0x20; glyph[2] = 0x40; glyph[3] = 0x20; glyph[4] = 0x1F; return true;
    case 'W':
      glyph[0] = 0x7F; glyph[1] = 0x20; glyph[2] = 0x18; glyph[3] = 0x20; glyph[4] = 0x7F; return true;
    case '-':
      glyph[0] = 0x08; glyph[1] = 0x08; glyph[2] = 0x08; glyph[3] = 0x08; glyph[4] = 0x08; return true;
    case ' ':
      return true;
    default:
      return false;
  }
}

void drawChar5x7(int x, int y, char c, uint8_t color, Rotation rotation) {
  uint8_t glyph[5];
  lookupGlyph5x7(c, glyph);

  for (uint8_t col = 0; col < 5; ++col) {
    const uint8_t bits = glyph[col];
    for (uint8_t row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        setPixelRotated(x + col, y + row, color, rotation);
      }
    }
  }
}

void drawText5x7(int x, int y, const char* text, uint8_t color, Rotation rotation) {
  while (*text != '\0') {
    drawChar5x7(x, y, *text, color, rotation);
    x += 6;
    ++text;
  }
}

void drawOrientationPattern(Rotation rotation, const char* label) {
  clearBuffer(Oled::COLOR_BLACK);
  drawRect(0, 0, 128, 64, Oled::COLOR_WHITE, rotation);
  drawRect(2, 2, 124, 60, Oled::COLOR_WHITE, rotation);
  drawLine(0, 0, 127, 63, Oled::COLOR_WHITE, rotation);
  drawLine(127, 0, 0, 63, Oled::COLOR_WHITE, rotation);
  fillRect(4, 4, 10, 10, Oled::COLOR_WHITE, rotation);
  fillRect(114, 50, 10, 10, Oled::COLOR_WHITE, rotation);
  drawText5x7(18, 12, "WAVESHARE", Oled::COLOR_WHITE, rotation);
  drawText5x7(34, 26, "OLED", Oled::COLOR_WHITE, rotation);
  drawText5x7(18, 40, label, Oled::COLOR_WHITE, rotation);
  sendFramebuffer();
}

void showFullWhite() {
  clearBuffer(Oled::COLOR_WHITE);
  sendFramebuffer();
}

void showFullBlack() {
  clearBuffer(Oled::COLOR_BLACK);
  sendFramebuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(Pins::OLED_CS, OUTPUT);
  pinMode(Pins::OLED_DC, OUTPUT);
  pinMode(Pins::OLED_RST, OUTPUT);
  digitalWrite(Pins::OLED_CS, HIGH);
  digitalWrite(Pins::OLED_DC, HIGH);
  digitalWrite(Pins::OLED_RST, HIGH);

  SPI1.begin();
  initDisplay();

  Serial.println("OLED DIAG START");

  showFullBlack();
  delay(250);
  showFullWhite();
  Serial.println("STEP 1: FULL WHITE");
  delay(1200);

  showFullBlack();
  delay(250);
  drawOrientationPattern(ROTATE_270, "ROT270");
  Serial.println("STEP 2: ROT270");
  delay(2000);

  drawOrientationPattern(ROTATE_90, "ROT090");
  Serial.println("STEP 3: ROT090");
  delay(2000);

  drawOrientationPattern(ROTATE_0, "ROT000");
  Serial.println("STEP 4: ROT000");
  delay(2000);

  drawOrientationPattern(ROTATE_180, "ROT180");
  Serial.println("STEP 5: ROT180");
  delay(2000);

  drawOrientationPattern(ROTATE_270, "FINAL");
  Serial.println("OLED DIAG FINAL");
}

void loop() {
}
