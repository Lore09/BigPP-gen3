#pragma once

// ---------- Pins (match docs/wiring.drawio) ----------
#define PIN_TRIG   2   // HC-SR04 TRIG
#define PIN_ECHO   3   // HC-SR04 ECHO
#define PIN_WS     6   // WS2812 DIN (through R2 330R)
#define PIN_RELAY  7   // Q1 base (through R1 1k) -> K1 coil

// ---------- Trigger behaviour ----------
#define TRIGGER_DISTANCE_CM   50      // something closer than this toggles the light (off -> on, on -> off)
#define TRIGGER_CONFIRM      3       // consecutive readings needed to toggle / re-arm (filters glitches)
#define MEASURE_INTERVAL_MS   60      // HC-SR04 datasheet: >= 60 ms between pings
#define ECHO_TIMEOUT_US       25000UL // ~4 m; no echo within this = nothing in range

// ---------- WS2812 ----------
#define NUM_LEDS          8
#define WS_BRIGHTNESS     80   // 0-255
#define WS_MAX_MA         400  // FastLED limits brightness to stay under this current

// ---------- Approach fade ----------
// Between TRIGGER_DISTANCE_CM + FADE_ZONE_CM and TRIGGER_DISTANCE_CM the LEDs preview the toggle:
// off -> brightening up to 100% at the trigger, on -> dimming down to 0% at the trigger.
// After a toggle you must leave the zone (go beyond its outer edge) before the next one.
#define FADE_ZONE_CM      10   // width of the fade zone in front of the trigger distance
#define FADE_STEPS        0    // 0 = continuous; N = quantize the zone into N visible brightness steps
#define FADE_SLEW_MS      300  // time for a full 0 -> 100% change; smooths sensor jitter (lower = snappier)
#define FRAME_MS          10   // LED refresh interval

// ---------- Fire effect (shown when on, and dimmed in the fade zone) ----------
// Each LED follows its own smooth noise curve, mapped onto FastLED's heat palette
// (0 = black, ~85 = red, ~170 = yellow, 255 = white).
#define FIRE_HEAT_MIN     70   // coolest colour (deep red/orange)
#define FIRE_HEAT_MAX     200  // hottest colour (bright yellow); push towards 255 for white-hot tips
#define FIRE_SPEED        3    // flicker speed (higher = faster, more nervous flame)
#define FIRE_SPREAD       90   // how different neighbouring LEDs are (low = LEDs flicker together)

// ---------- Debug ----------
#define DEBUG_SERIAL 1 // 1 = print distance/state on the serial monitor (115200)
