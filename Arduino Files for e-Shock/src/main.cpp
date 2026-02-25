/*
 * e-Shock LED Controller — Arduino Nano
 * 
 * Pin 3  : Shock output (rising-edge trigger, active HIGH)
 * Pin 8  : WS2812B individually-addressable LED data line
 * 
 * Serial protocol (115200 baud):
 *   Commands IN  (from Python) ─────────────────────────────────────────
 *     CONFIG_LED:<num_leds>,<brightness>,<r>,<g>,<b>,<led_on_ms>,<led_off_ms>
 *     CONFIG_SHOCK:<shock_on_ms>,<shock_off_ms>,<num_cycles>
 *     PREVIEW          — run LEDs + report shock times (pin stays LOW)
 *     RECORD           — run LEDs + actually drive shock pin
 *     STOP             — halt everything immediately
 *
 *   Messages OUT (to Python) ───────────────────────────────────────────
 *     OK               — config accepted
 *     SHOCK_ON         — shock (or simulated shock) just started
 *     SHOCK_OFF        — shock (or simulated shock) just ended
 *     LED_ON           — LED pattern turned on
 *     LED_OFF          — LED pattern turned off
 *     SEQUENCE_DONE    — full sequence finished
 *     ERR:<message>    — something went wrong
 */

#include <Arduino.h>
#include <FastLED.h>

// ── Hardware pins ──────────────────────────────────────────────────────
#define SHOCK_PIN   3
#define LED_PIN     8
#define MAX_LEDS  144          // physical max; actual count set at runtime

// ── LED buffer ─────────────────────────────────────────────────────────
CRGB leds[MAX_LEDS];

// ── Configurable parameters (defaults) ─────────────────────────────────
uint16_t numLeds      = 8;
uint8_t  ledBright    = 128;
uint8_t  ledR         = 255;
uint8_t  ledG         = 0;
uint8_t  ledB         = 0;
uint32_t ledOnMs      = 500;
uint32_t ledOffMs     = 500;

uint32_t shockOnMs    = 200;
uint32_t shockOffMs   = 800;
uint16_t shockCycles  = 5;

// ── State machine ──────────────────────────────────────────────────────
enum Mode { IDLE, PREVIEW, RECORDING };
volatile Mode currentMode = IDLE;

// Timing state
uint32_t seqStartMs       = 0;
uint16_t shockCyclesDone  = 0;
bool     shockActive      = false;
bool     ledsOn           = false;
uint32_t lastShockToggle  = 0;
uint32_t lastLedToggle    = 0;
bool     sequenceRunning  = false;

// ── Forward declarations ───────────────────────────────────────────────
void handleSerial();
void parseConfig(const String &line);
void startSequence(Mode mode);
void stopSequence();
void runSequence();

// ═══════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(SHOCK_PIN, OUTPUT);
  digitalWrite(SHOCK_PIN, LOW);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(ledBright);
  fill_solid(leds, MAX_LEDS, CRGB::Black);
  FastLED.show();

  Serial.println("READY");
}

// ═══════════════════════════════════════════════════════════════════════
void loop() {
  handleSerial();

  if (sequenceRunning) {
    runSequence();
  }
}

// ── Serial command router ──────────────────────────────────────────────
void handleSerial() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("CONFIG_LED:") || line.startsWith("CONFIG_SHOCK:")) {
    parseConfig(line);
  } else if (line == "PREVIEW") {
    startSequence(PREVIEW);
  } else if (line == "RECORD") {
    startSequence(RECORDING);
  } else if (line == "STOP") {
    stopSequence();
  } else {
    Serial.print("ERR:Unknown command ");
    Serial.println(line);
  }
}

// ── Parse & apply configuration ────────────────────────────────────────
void parseConfig(const String &line) {
  if (line.startsWith("CONFIG_LED:")) {
    // CONFIG_LED:<num>,<bright>,<r>,<g>,<b>,<on_ms>,<off_ms>
    String payload = line.substring(11);
    int idx = 0;
    int parts[7];
    int from = 0;
    for (int i = 0; i < 7; i++) {
      int sep = payload.indexOf(',', from);
      if (sep == -1) sep = payload.length();
      parts[i] = payload.substring(from, sep).toInt();
      from = sep + 1;
    }
    numLeds   = constrain(parts[0], 1, MAX_LEDS);
    ledBright = constrain(parts[1], 0, 255);
    ledR      = constrain(parts[2], 0, 255);
    ledG      = constrain(parts[3], 0, 255);
    ledB      = constrain(parts[4], 0, 255);
    ledOnMs   = (uint32_t)parts[5];
    ledOffMs  = (uint32_t)parts[6];

    FastLED.setBrightness(ledBright);
    Serial.println("OK");

  } else if (line.startsWith("CONFIG_SHOCK:")) {
    // CONFIG_SHOCK:<on_ms>,<off_ms>,<cycles>
    String payload = line.substring(13);
    int parts[3];
    int from = 0;
    for (int i = 0; i < 3; i++) {
      int sep = payload.indexOf(',', from);
      if (sep == -1) sep = payload.length();
      parts[i] = payload.substring(from, sep).toInt();
      from = sep + 1;
    }
    shockOnMs   = (uint32_t)parts[0];
    shockOffMs  = (uint32_t)parts[1];
    shockCycles = (uint16_t)parts[2];
    Serial.println("OK");
  }
}

// ── Start a sequence ───────────────────────────────────────────────────
void startSequence(Mode mode) {
  stopSequence();                       // clean slate
  currentMode      = mode;
  sequenceRunning  = true;
  seqStartMs       = millis();
  shockCyclesDone  = 0;
  shockActive      = false;
  ledsOn           = false;
  lastShockToggle  = millis();
  lastLedToggle    = millis();

  // Immediately turn LEDs on & start first shock
  fill_solid(leds, numLeds, CRGB(ledR, ledG, ledB));
  FastLED.show();
  ledsOn = true;
  Serial.println("LED_ON");

  if (shockCycles > 0) {
    if (currentMode == RECORDING) {
      digitalWrite(SHOCK_PIN, HIGH);
    }
    shockActive = true;
    Serial.println("SHOCK_ON");
  }

  Serial.println("OK");
}

// ── Stop everything ────────────────────────────────────────────────────
void stopSequence() {
  sequenceRunning = false;
  currentMode     = IDLE;

  digitalWrite(SHOCK_PIN, LOW);
  shockActive = false;

  fill_solid(leds, MAX_LEDS, CRGB::Black);
  FastLED.show();
  ledsOn = false;
}

// ── Non-blocking sequence runner (called every loop) ───────────────────
void runSequence() {
  uint32_t now = millis();

  // ─── Shock cycling ───────────────────────────────────────────────
  if (shockCyclesDone < shockCycles) {
    if (shockActive) {
      if (now - lastShockToggle >= shockOnMs) {
        // Turn shock OFF
        if (currentMode == RECORDING) {
          digitalWrite(SHOCK_PIN, LOW);
        }
        shockActive     = false;
        lastShockToggle = now;
        shockCyclesDone++;
        Serial.println("SHOCK_OFF");
      }
    } else {
      if (now - lastShockToggle >= shockOffMs) {
        // Turn shock ON for next cycle
        if (currentMode == RECORDING) {
          digitalWrite(SHOCK_PIN, HIGH);
        }
        shockActive     = true;
        lastShockToggle = now;
        Serial.println("SHOCK_ON");
      }
    }
  }

  // ─── LED cycling (runs independently, continues even after shocks done) ──
  if (ledsOn) {
    if (now - lastLedToggle >= ledOnMs) {
      fill_solid(leds, numLeds, CRGB::Black);
      FastLED.show();
      ledsOn        = false;
      lastLedToggle = now;
      Serial.println("LED_OFF");
    }
  } else {
    if (now - lastLedToggle >= ledOffMs) {
      fill_solid(leds, numLeds, CRGB(ledR, ledG, ledB));
      FastLED.show();
      ledsOn        = true;
      lastLedToggle = now;
      Serial.println("LED_ON");
    }
  }

  // ─── Sequence completion ─────────────────────────────────────────
  if (shockCyclesDone >= shockCycles && !shockActive) {
    // Shocks are done — keep LEDs cycling until Python sends STOP.
    // But notify once:
    static bool doneSent = false;
    if (!doneSent) {
      Serial.println("SEQUENCE_DONE");
      doneSent = true;
    }
  }
}