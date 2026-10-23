#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

#if FADE_ZONE_CM <= 0
#error "FADE_ZONE_CM must be > 0"
#endif

#define FADE_OUTER_CM (TRIGGER_DISTANCE_CM + FADE_ZONE_CM)
// brightness change per frame so that 0 -> 255 takes FADE_SLEW_MS
#define FADE_STEP_PER_FRAME max(1, 255L * FRAME_MS / FADE_SLEW_MS)

CRGB leds[NUM_LEDS];

bool lightOn = false;
bool armed = true;          // false right after a toggle, until the object leaves the fade zone

unsigned long lastMeasure = 0;
unsigned long lastFrame = 0;
uint16_t lastCm = 0;
uint8_t inRangeCount = 0;
uint8_t outsideCount = 0;
uint8_t level = 0;          // current LED level 0-255, slews towards targetLevel()
bool intro = false;         // turn-on animation running
unsigned long introStart = 0;

// Returns distance in cm, or 0 if nothing is in range.
uint16_t readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  return us / 58; // speed of sound, round trip: ~58 us per cm
}

void setRelay(bool on) {
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}

void updateSensor(unsigned long now) {
  if (now - lastMeasure < MEASURE_INTERVAL_MS) return;
  lastMeasure = now;

  uint16_t cm = readDistanceCm();
  lastCm = cm;

  bool inRange = cm > 0 && cm < TRIGGER_DISTANCE_CM;
  bool outside = cm == 0 || cm >= FADE_OUTER_CM;
  inRangeCount = inRange ? min(inRangeCount + 1, 255) : 0;
  outsideCount = outside ? min(outsideCount + 1, 255) : 0;

  if (!armed && outsideCount >= TRIGGER_CONFIRM) armed = true;

  // toggle: each confirmed entry into the trigger distance swaps the light state
  if (armed && inRangeCount >= TRIGGER_CONFIRM) {
    lightOn = !lightOn;
    armed = false;
    setRelay(lightOn);
    intro = lightOn && INTRO_MS > 0;
    introStart = now;
  }

#if DEBUG_SERIAL
  Serial.print(F("dist="));
  Serial.print(cm);
  Serial.print(F("cm light="));
  Serial.print(lightOn ? F("ON") : F("OFF"));
  Serial.print(F(" armed="));
  Serial.print(armed);
  Serial.print(F(" level="));
  Serial.println(level);
#endif
}

// 255 when on / 0 when off, or the toggle preview while an object is in the fade zone.
uint8_t targetLevel() {
  uint8_t base = lightOn ? 255 : 0;
  if (!armed || lastCm == 0 || lastCm >= FADE_OUTER_CM) return base;

  // progress through the zone: 0 at the outer edge -> 255 at the trigger distance
  uint16_t into = FADE_OUTER_CM - max(lastCm, (uint16_t)TRIGGER_DISTANCE_CM);
  uint8_t p = into * 255UL / FADE_ZONE_CM;
#if FADE_STEPS > 0
  p = (uint16_t)(p * FADE_STEPS / 255) * 255 / FADE_STEPS;
#endif
  return lightOn ? 255 - p : p;
}

// slow breathing glow, phase-shifted per LED, with a gentle noise-driven hue drift
void drawGlow(unsigned long now) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t v = beatsin8(GLOW_BPM, GLOW_MIN, 255, 0, i * GLOW_PHASE);
    uint8_t hue = GLOW_HUE - GLOW_HUE_DRIFT / 2 + scale8(inoise8(i * 50, now / 8), GLOW_HUE_DRIFT);
    leds[i] = CHSV(hue, GLOW_SAT, v);
  }
}

#if INTRO_MS > 0
// turn-on animation: a comet laps the ring, decelerating, while the glow fades in underneath
void drawIntro(unsigned long now, unsigned long elapsed) {
  uint8_t t = elapsed * 255UL / INTRO_MS;
  uint8_t eased = 255 - scale8(255 - t, 255 - t);       // ease-out: fast start, gentle stop
  uint8_t mix = ease8InOutCubic(t);                      // 0 = all comet, 255 = all glow

  const uint16_t span = NUM_LEDS * 256;                  // ring length in 1/256 LED steps
  const uint16_t tail = INTRO_TAIL * 256;
  uint16_t head = (uint32_t)eased * INTRO_TURNS * span / 255 % span;

  drawGlow(now);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t led = INTRO_REVERSE ? NUM_LEDS - 1 - i : i;
    uint16_t behind = (head + span - i * 256) % span;    // distance from the comet head
    uint8_t v = behind < tail ? 255 - (uint32_t)behind * 255 / tail : 0;
    leds[led].nscale8(mix);
    leds[led] += CHSV(GLOW_HUE, INTRO_HEAD_SAT, scale8(v, 255 - mix));
  }
}
#endif

void updateLeds(unsigned long now) {
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;

  uint8_t target = targetLevel();
  if (level < target) level = min(level + FADE_STEP_PER_FRAME, (long)target);
  else if (level > target) level = max(level - FADE_STEP_PER_FRAME, (long)target);

#if INTRO_MS > 0
  if (intro && now - introStart >= INTRO_MS) intro = false;
  if (intro) drawIntro(now, now - introStart);
  else
#endif
  // glow, scaled by the fade level
  drawGlow(now);
  FastLED.setBrightness((uint16_t)WS_BRIGHTNESS * level / 255);
  FastLED.show();
}

void setup() {
  pinMode(PIN_RELAY, OUTPUT);
  setRelay(false);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

#if DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println(F("BigPP gen3 ready"));
#endif

  FastLED.addLeds<WS2812B, PIN_WS, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(0);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, WS_MAX_MA);
  FastLED.clear(true);
}

void loop() {
  unsigned long now = millis();
  updateSensor(now);
  updateLeds(now);
}
