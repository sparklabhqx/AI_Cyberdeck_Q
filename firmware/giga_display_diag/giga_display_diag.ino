#include "Arduino_H7_Video.h"
#include "ArduinoGraphics.h"

Arduino_H7_Video Display(800, 480, GigaDisplayShield);

namespace Pins {
const pin_size_t BUTTON_PIN = D2;
}

bool lastReading = HIGH;
bool stableState = HIGH;
bool screenOn = true;
unsigned long lastEdgeMs = 0;
unsigned long lastHeartbeatMs = 0;

void drawScreen() {
  Display.beginDraw();
  if (screenOn) {
    Display.background(0x001400);
    Display.clear();
    Display.stroke(0x5cff72);
    Display.noFill();
    Display.rect(18, 18, Display.width() - 36, Display.height() - 36);
    Display.textFont(Font_5x7);
    Display.textSize(4);
    Display.text("GIGA DISPLAY DIAG", 40, 70);
    Display.textSize(3);
    Display.text("Display.begin OK", 40, 150);
    Display.text("Button D2 toggles screen", 40, 210);
    Display.text("USB serial @115200", 40, 270);
  } else {
    Display.background(0x000000);
    Display.clear();
  }
  Display.endDraw();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(Pins::BUTTON_PIN, INPUT_PULLUP);
  delay(250);

  Serial.println("[DIAG] boot");
  if (Display.begin() != 0) {
    Serial.println("[DIAG] Display.begin failed");
    pinMode(LEDR, OUTPUT);
    while (true) {
      digitalWrite(LEDR, LOW);
      delay(150);
      digitalWrite(LEDR, HIGH);
      delay(150);
    }
  }

  Serial.println("[DIAG] Display.begin ok");
  drawScreen();
}

void loop() {
  const unsigned long now = millis();
  const bool reading = digitalRead(Pins::BUTTON_PIN);

  if (reading != lastReading) {
    lastEdgeMs = now;
    lastReading = reading;
  }

  if (now - lastEdgeMs >= 45 && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      screenOn = !screenOn;
      digitalWrite(LED_BUILTIN, screenOn ? HIGH : LOW);
      Serial.print("[DIAG] button toggle, screenOn=");
      Serial.println(screenOn ? "true" : "false");
      drawScreen();
    }
  }

  if (now - lastHeartbeatMs >= 3000) {
    lastHeartbeatMs = now;
    Serial.print("[DIAG] heartbeat, button=");
    Serial.println(stableState == LOW ? "pressed" : "released");
  }
}
