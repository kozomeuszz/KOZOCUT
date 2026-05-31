#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "logotyp_web_jpg.h"

namespace Defaults {
constexpr float STEPS_PER_MM = 44.0f;
constexpr uint32_t FEED_SPEED_PERCENT = 85;
constexpr uint32_t CUT_SPEED_PERCENT = 90;
constexpr uint32_t STRIPPER_SPEED_PERCENT = 100;
constexpr uint32_t FEED_FAST_DELAY_US = 100;
constexpr uint32_t FEED_SLOW_DELAY_US = 2000;
constexpr uint32_t CUT_FAST_DELAY_US = 100;
constexpr uint32_t CUT_SLOW_DELAY_US = 3000;
constexpr uint32_t STRIPPER_FAST_DELAY_US = 10;
constexpr uint32_t STRIPPER_SLOW_DELAY_US = 714;
constexpr uint32_t GUILLOTINE_RAMP_MIN_TIME_US = 150000;
constexpr uint32_t GUILLOTINE_RAMP_MAX_TIME_US = 250000;
constexpr uint32_t GUILLOTINE_RAMP_START_DELAY_US = 1200;
constexpr uint32_t GUILLOTINE_RAMP_EXTRA_DELAY_US = 800;
constexpr uint32_t PIECES = 1;
constexpr float LENGTH_MM = 100.0f;
constexpr uint32_t GUILLOTINE_STEPS_PER_CUT = 1600;
constexpr bool EXTRUDER_INVERT_DIR = false;
constexpr bool GUILLOTINE_INVERT_DIR = false;
}

namespace Pins {
constexpr uint8_t EXTRUDER_DIR = 0;
constexpr uint8_t EXTRUDER_STEP = 1;
constexpr uint8_t GUILLOTINE_DIR = 7;
constexpr uint8_t GUILLOTINE_STEP = 3;
constexpr uint8_t DRIVER_ENABLE = 4;
constexpr uint8_t BLUE_STATUS_LED = 8;
constexpr uint8_t BOOT_BUTTON = 9;
}

struct MachineConfig {
  float stepsPerMm = Defaults::STEPS_PER_MM;
  uint32_t feedSpeedPercent = Defaults::FEED_SPEED_PERCENT;
  uint32_t cutSpeedPercent = Defaults::CUT_SPEED_PERCENT;
  uint32_t pieces = Defaults::PIECES;
  float lengthMm = Defaults::LENGTH_MM;
  uint32_t guillotineStepsPerCut = Defaults::GUILLOTINE_STEPS_PER_CUT;
  bool extruderInvertDir = Defaults::EXTRUDER_INVERT_DIR;
  bool guillotineInvertDir = Defaults::GUILLOTINE_INVERT_DIR;
};

struct MachineState {
  bool running = false;
  bool stopRequested = false;
  bool paused = false;
  uint32_t currentPiece = 0;
  uint32_t completedPieces = 0;
  String status = "idle";
};

struct StripperState {
  bool enabled = false;
  bool invertDir = false;
  uint32_t speedPercent = Defaults::STRIPPER_SPEED_PERCENT;
  bool stepHigh = false;
  unsigned long lastToggleUs = 0;
};

constexpr char kApSsid[] = "KOZOcut";
const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApSubnet(255, 255, 255, 0);
constexpr byte kDnsPort = 53;

DNSServer dnsServer;
WebServer server(80);
Preferences preferences;
MachineConfig config;
MachineConfig savedConfig;
MachineState state;
StripperState stripper;
uint32_t savedStripperSpeedPercent = Defaults::STRIPPER_SPEED_PERCENT;
bool blueLedBlinkOn = false;
unsigned long lastLedBlinkMs = 0;
bool lastBootButtonReading = HIGH;
bool bootButtonStableState = HIGH;
unsigned long lastBootButtonChangeMs = 0;
String pausedResumeStatus = "running";

void setDriversEnabled(bool enabled);

uint32_t clampPercent(uint32_t value) {
  return constrain(value, 1U, 100U);
}

uint32_t speedPercentToDelayUs(uint32_t percent, uint32_t slowDelayUs,
                               uint32_t fastDelayUs) {
  const uint32_t clampedPercent = clampPercent(percent);
  if (slowDelayUs <= fastDelayUs) {
    return fastDelayUs;
  }

  const uint32_t range = slowDelayUs - fastDelayUs;
  const uint32_t scaled = static_cast<uint32_t>(
      (static_cast<uint64_t>(range) * (clampedPercent - 1U)) / 99U);
  return slowDelayUs - scaled;
}

uint32_t legacyDelayUsToSpeedPercent(uint32_t delayUs, uint32_t slowDelayUs,
                                     uint32_t fastDelayUs) {
  if (delayUs <= 100U) {
    return clampPercent(delayUs);
  }
  if (delayUs >= slowDelayUs) {
    return 1U;
  }
  if (delayUs <= fastDelayUs) {
    return 100U;
  }

  const uint32_t range = slowDelayUs - fastDelayUs;
  const uint32_t fromFast = slowDelayUs - delayUs;
  const uint32_t scaled = static_cast<uint32_t>(
      (static_cast<uint64_t>(fromFast) * 99U) / range);
  return clampPercent(1U + scaled);
}

uint32_t feedDelayUs() {
  return speedPercentToDelayUs(config.feedSpeedPercent,
                               Defaults::FEED_SLOW_DELAY_US,
                               Defaults::FEED_FAST_DELAY_US);
}

uint32_t cutDelayUs() {
  return speedPercentToDelayUs(config.cutSpeedPercent,
                               Defaults::CUT_SLOW_DELAY_US,
                               Defaults::CUT_FAST_DELAY_US);
}

uint32_t stripperDelayUs() {
  if (stripper.speedPercent == 0) {
    return 0;
  }
  return speedPercentToDelayUs(clampPercent(stripper.speedPercent),
                               Defaults::STRIPPER_SLOW_DELAY_US,
                               Defaults::STRIPPER_FAST_DELAY_US);
}

uint32_t guillotineRampTimeUs() {
  const uint32_t clampedPercent = clampPercent(config.cutSpeedPercent);
  const uint32_t rangeUs =
      Defaults::GUILLOTINE_RAMP_MAX_TIME_US -
      Defaults::GUILLOTINE_RAMP_MIN_TIME_US;
  const uint32_t scaled = static_cast<uint32_t>(
      (static_cast<uint64_t>(rangeUs) * (clampedPercent - 1U)) / 99U);
  return Defaults::GUILLOTINE_RAMP_MAX_TIME_US - scaled;
}

void applyDefaultConfig() {
  config.stepsPerMm = Defaults::STEPS_PER_MM;
  config.feedSpeedPercent = Defaults::FEED_SPEED_PERCENT;
  config.cutSpeedPercent = Defaults::CUT_SPEED_PERCENT;
  config.pieces = Defaults::PIECES;
  config.lengthMm = Defaults::LENGTH_MM;
  config.guillotineStepsPerCut = Defaults::GUILLOTINE_STEPS_PER_CUT;
  config.extruderInvertDir = Defaults::EXTRUDER_INVERT_DIR;
  config.guillotineInvertDir = Defaults::GUILLOTINE_INVERT_DIR;
}

void loadConfigFromNvs() {
  applyDefaultConfig();

  preferences.begin("kozocut", true);
  config.stepsPerMm = preferences.getFloat("stepsPerMm", config.stepsPerMm);
  if (preferences.isKey("feedPct")) {
    config.feedSpeedPercent =
        clampPercent(preferences.getUInt("feedPct", config.feedSpeedPercent));
  } else {
    config.feedSpeedPercent = legacyDelayUsToSpeedPercent(
        preferences.getUInt("feedDelay", Defaults::FEED_SLOW_DELAY_US),
        Defaults::FEED_SLOW_DELAY_US, Defaults::FEED_FAST_DELAY_US);
  }
  if (preferences.isKey("cutPct")) {
    config.cutSpeedPercent =
        clampPercent(preferences.getUInt("cutPct", config.cutSpeedPercent));
  } else {
    config.cutSpeedPercent = legacyDelayUsToSpeedPercent(
      preferences.getUInt("cutDelay", Defaults::CUT_SLOW_DELAY_US),
      Defaults::CUT_SLOW_DELAY_US, Defaults::CUT_FAST_DELAY_US);
  }
  stripper.speedPercent = clampPercent(
      preferences.getUInt("stripPct", Defaults::STRIPPER_SPEED_PERCENT));
  savedStripperSpeedPercent = stripper.speedPercent;
  config.pieces = preferences.getUInt("pieces", config.pieces);
  config.lengthMm = preferences.getFloat("lengthMm", config.lengthMm);
  config.guillotineStepsPerCut =
      preferences.getUInt("cutSteps", config.guillotineStepsPerCut);
  config.extruderInvertDir =
      preferences.getBool("extInv", config.extruderInvertDir);
  config.guillotineInvertDir =
      preferences.getBool("guiInv", config.guillotineInvertDir);
  preferences.end();
  savedConfig = config;
}

void saveConfigToNvs() {
  preferences.begin("kozocut", false);
  preferences.putFloat("stepsPerMm", config.stepsPerMm);
  preferences.putUInt("feedPct", config.feedSpeedPercent);
  preferences.putUInt("cutPct", config.cutSpeedPercent);
  preferences.putUInt("stripPct", stripper.speedPercent);
  preferences.putUInt("pieces", config.pieces);
  preferences.putFloat("lengthMm", config.lengthMm);
  preferences.putUInt("cutSteps", config.guillotineStepsPerCut);
  preferences.putBool("extInv", config.extruderInvertDir);
  preferences.putBool("guiInv", config.guillotineInvertDir);
  preferences.end();
  savedConfig = config;
  savedStripperSpeedPercent = stripper.speedPercent;
}

bool isMotorActive() {
  return state.running || state.status == "feeding" || state.status == "cutting" ||
         state.status == "jog";
}

void disableStripper() {
  stripper.enabled = false;
  stripper.stepHigh = false;
  stripper.lastToggleUs = 0;
  digitalWrite(Pins::EXTRUDER_STEP, LOW);
  if (!isMotorActive()) {
    setDriversEnabled(false);
  }
}

void togglePauseFromBootButton() {
  if (!isMotorActive() && !stripper.enabled && !state.paused) {
    stripper.enabled = true;
    stripper.stepHigh = false;
    stripper.lastToggleUs = 0;
    state.paused = false;
    state.status = "idle";
    return;
  }

  if (!state.paused) {
    state.paused = true;
    pausedResumeStatus = state.status;
    state.status = "paused";
    return;
  }

  state.paused = false;
  state.status = pausedResumeStatus.length() ? pausedResumeStatus : "running";
}

bool togglePauseState() {
  if (!state.running && !state.paused) {
    return false;
  }
  togglePauseFromBootButton();
  return true;
}

void pollBootButton() {
  const bool reading = digitalRead(Pins::BOOT_BUTTON);
  const unsigned long now = millis();

  if (reading != lastBootButtonReading) {
    lastBootButtonChangeMs = now;
    lastBootButtonReading = reading;
  }

  if ((now - lastBootButtonChangeMs) >= 35 && reading != bootButtonStableState) {
    bootButtonStableState = reading;
    if (bootButtonStableState == LOW) {
      togglePauseFromBootButton();
    }
  }
}

void serviceStripper() {
  if (!stripper.enabled || isMotorActive() || state.paused) {
    if (stripper.stepHigh) {
      stripper.stepHigh = false;
      digitalWrite(Pins::EXTRUDER_STEP, LOW);
    }
    if (!stripper.enabled && !isMotorActive()) {
      setDriversEnabled(false);
    }
    return;
  }

  const uint32_t delayUs = stripperDelayUs();
  if (delayUs == 0) {
    if (stripper.stepHigh) {
      stripper.stepHigh = false;
      digitalWrite(Pins::EXTRUDER_STEP, LOW);
    }
    return;
  }

  setDriversEnabled(true);
  digitalWrite(Pins::EXTRUDER_DIR, stripper.invertDir ? LOW : HIGH);

  const unsigned long nowUs = micros();
  if (stripper.lastToggleUs == 0) {
    stripper.lastToggleUs = nowUs;
  }

  uint8_t catchUpToggles = 0;
  while (static_cast<uint32_t>(nowUs - stripper.lastToggleUs) >= delayUs &&
         catchUpToggles < 8) {
    stripper.lastToggleUs += delayUs;
    stripper.stepHigh = !stripper.stepHigh;
    digitalWrite(Pins::EXTRUDER_STEP, stripper.stepHigh ? HIGH : LOW);
    ++catchUpToggles;
  }
}

void setBlueLed(bool on) {
  digitalWrite(Pins::BLUE_STATUS_LED, on ? LOW : HIGH);
}

void updateStatusLed() {
  const unsigned long now = millis();

  if (state.paused || state.status == "paused") {
    blueLedBlinkOn = false;
    setBlueLed(true);
    return;
  }

  if (isMotorActive()) {
    if (now - lastLedBlinkMs >= 180) {
      lastLedBlinkMs = now;
      blueLedBlinkOn = !blueLedBlinkOn;
      setBlueLed(blueLedBlinkOn);
    }
    return;
  }

  blueLedBlinkOn = false;
  setBlueLed(false);
}

String jsonString(const String &value) {
  String escaped = value;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  return "\"" + escaped + "\"";
}

void setDriversEnabled(bool enabled) {
  digitalWrite(Pins::DRIVER_ENABLE, enabled ? LOW : HIGH);
}

void pulseStepPin(uint8_t pin, uint32_t delayUs) {
  digitalWrite(pin, HIGH);
  delayMicroseconds(delayUs);
  digitalWrite(pin, LOW);
  delayMicroseconds(delayUs);
}

bool waitWhilePaused() {
  while (state.paused) {
    pollBootButton();
    dnsServer.processNextRequest();
    server.handleClient();
    updateStatusLed();
    if (state.stopRequested) {
      state.status = "stopped";
      return false;
    }
    delay(2);
  }
  return true;
}

bool stepMotor(uint8_t dirPin, uint8_t stepPin, bool invertDir, bool forward,
               uint32_t steps, uint32_t delayUs) {
  const bool dirLevel = (forward ^ invertDir) ? HIGH : LOW;
  digitalWrite(dirPin, dirLevel);

  for (uint32_t i = 0; i < steps; ++i) {
    pollBootButton();
    if (!waitWhilePaused()) {
      return false;
    }
    if (state.stopRequested) {
      state.status = "stopped";
      return false;
    }
    pulseStepPin(stepPin, delayUs);
    if ((i & 0x3F) == 0) {
      dnsServer.processNextRequest();
      server.handleClient();
      updateStatusLed();
      yield();
    }
  }
  return true;
}

bool stepMotorWithRamp(uint8_t dirPin, uint8_t stepPin, bool invertDir,
                       bool forward, uint32_t steps, uint32_t targetDelayUs,
                       uint32_t startDelayUs, uint32_t rampTimeUs) {
  if (steps == 0) {
    return true;
  }

  const bool dirLevel = (forward ^ invertDir) ? HIGH : LOW;
  digitalWrite(dirPin, dirLevel);

  const uint32_t clampedStartDelayUs = max(startDelayUs, targetDelayUs);
  const uint32_t averageDelayUs = (clampedStartDelayUs + targetDelayUs) / 2U;
  const uint32_t estimatedRampSteps =
      max<uint32_t>(1U, rampTimeUs / max<uint32_t>(1U, 2U * averageDelayUs));
  const uint32_t rampSteps = min(steps, estimatedRampSteps);
  const uint32_t rampRangeUs = clampedStartDelayUs - targetDelayUs;

  for (uint32_t i = 0; i < steps; ++i) {
    pollBootButton();
    if (!waitWhilePaused()) {
      return false;
    }
    if (state.stopRequested) {
      state.status = "stopped";
      return false;
    }

    uint32_t stepDelayUs = targetDelayUs;
    if (i < rampSteps && rampSteps > 1U) {
      stepDelayUs = clampedStartDelayUs -
                    static_cast<uint32_t>((static_cast<uint64_t>(rampRangeUs) * i) /
                                          (rampSteps - 1U));
    }

    pulseStepPin(stepPin, stepDelayUs);
    if ((i & 0x3F) == 0) {
      dnsServer.processNextRequest();
      server.handleClient();
      updateStatusLed();
      yield();
    }
  }
  return true;
}

bool feedCableMm(float lengthMm, bool forward) {
  if (lengthMm <= 0.0f || config.stepsPerMm <= 0.0f) {
    return true;
  }
  const uint32_t steps = lroundf(lengthMm * config.stepsPerMm);
  return stepMotor(Pins::EXTRUDER_DIR, Pins::EXTRUDER_STEP,
                   config.extruderInvertDir, forward, steps,
                   feedDelayUs());
}

bool runCutCycle() {
  const uint32_t targetDelayUs = cutDelayUs();
  const uint32_t startDelayUs =
      max(Defaults::GUILLOTINE_RAMP_START_DELAY_US,
          targetDelayUs + Defaults::GUILLOTINE_RAMP_EXTRA_DELAY_US);
  return stepMotorWithRamp(
      Pins::GUILLOTINE_DIR, Pins::GUILLOTINE_STEP,
      config.guillotineInvertDir, true, config.guillotineStepsPerCut,
      targetDelayUs, startDelayUs, guillotineRampTimeUs());
}

bool moveGuillotineSteps(uint32_t steps, bool forward) {
  const uint32_t targetDelayUs = cutDelayUs();
  const uint32_t startDelayUs =
      max(Defaults::GUILLOTINE_RAMP_START_DELAY_US,
          targetDelayUs + Defaults::GUILLOTINE_RAMP_EXTRA_DELAY_US);
  return stepMotorWithRamp(Pins::GUILLOTINE_DIR, Pins::GUILLOTINE_STEP,
                           config.guillotineInvertDir, forward, steps,
                           targetDelayUs, startDelayUs,
                           guillotineRampTimeUs());
}

void runProductionCycle() {
  disableStripper();
  state.running = true;
  state.stopRequested = false;
  state.paused = false;
  state.currentPiece = 0;
  state.completedPieces = 0;
  state.status = "running";
  pausedResumeStatus = "running";
  setDriversEnabled(true);

  for (uint32_t piece = 1; piece <= config.pieces; ++piece) {
    state.currentPiece = piece;
    state.status = "feeding";
    if (!feedCableMm(config.lengthMm, true)) {
      break;
    }

    for (uint32_t i = 0; i < 120; i += 2) {
      pollBootButton();
      if (!waitWhilePaused() || state.stopRequested) {
        break;
      }
      delay(2);
    }
    if (state.stopRequested) {
      break;
    }

    state.status = "cutting";
    if (!runCutCycle()) {
      break;
    }
    state.completedPieces = piece;

    for (uint32_t i = 0; i < 120; i += 2) {
      pollBootButton();
      if (!waitWhilePaused() || state.stopRequested) {
        break;
      }
      delay(2);
    }
    if (state.stopRequested) {
      break;
    }
  }

  setDriversEnabled(false);
  state.running = false;
  state.paused = false;
  if (state.stopRequested) {
    state.status = "stopped";
  } else {
    state.status = "idle";
  }
  state.stopRequested = false;
}

bool parseBooleanArg(const String &name, bool currentValue) {
  if (!server.hasArg(name)) {
    return currentValue;
  }
  const String value = server.arg(name);
  return value == "1" || value == "true" || value == "on";
}

void sendJsonStatus() {
  String body = "{";
  body += "\"status\":" + jsonString(state.status) + ",";
  body += "\"running\":" + String(state.running ? "true" : "false") + ",";
  body += "\"stopRequested\":" + String(state.stopRequested ? "true" : "false") + ",";
  body += "\"currentPiece\":" + String(state.currentPiece) + ",";
  body += "\"completedPieces\":" + String(state.completedPieces) + ",";
  body += "\"pieces\":" + String(config.pieces) + ",";
  body += "\"lengthMm\":" + String(config.lengthMm, 2) + ",";
  body += "\"stepsPerMm\":" + String(config.stepsPerMm, 4) + ",";
  body += "\"feedSpeedPercent\":" + String(config.feedSpeedPercent) + ",";
  body += "\"cutSpeedPercent\":" + String(config.cutSpeedPercent) + ",";
  body += "\"guillotineStepsPerCut\":" + String(config.guillotineStepsPerCut) + ",";
  body += "\"extruderInvertDir\":" + String(config.extruderInvertDir ? "true" : "false") + ",";
  body += "\"guillotineInvertDir\":" + String(config.guillotineInvertDir ? "true" : "false") + ",";
  body += "\"savedStepsPerMm\":" + String(savedConfig.stepsPerMm, 4) + ",";
  body += "\"savedFeedSpeedPercent\":" + String(savedConfig.feedSpeedPercent) + ",";
  body += "\"savedCutSpeedPercent\":" + String(savedConfig.cutSpeedPercent) + ",";
  body += "\"savedPieces\":" + String(savedConfig.pieces) + ",";
  body += "\"savedLengthMm\":" + String(savedConfig.lengthMm, 2) + ",";
  body += "\"savedGuillotineStepsPerCut\":" + String(savedConfig.guillotineStepsPerCut) + ",";
  body += "\"savedExtruderInvertDir\":" + String(savedConfig.extruderInvertDir ? "true" : "false") + ",";
  body += "\"savedGuillotineInvertDir\":" + String(savedConfig.guillotineInvertDir ? "true" : "false") + ",";
  body += "\"stripperEnabled\":" + String(stripper.enabled ? "true" : "false") + ",";
  body += "\"stripperInvertDir\":" + String(stripper.invertDir ? "true" : "false") + ",";
  body += "\"stripperSpeedPercent\":" + String(stripper.speedPercent) + ",";
  body += "\"savedStripperSpeedPercent\":" + String(savedStripperSpeedPercent) + ",";
  body += "\"apIp\":\"" + WiFi.softAPIP().toString() + "\"";
  body += "}";
  server.send(200, "application/json", body);
}

void handleRoot() {
  static const char page[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>KOZOcut</title>
  <style>
    :root {
      --bg: #f7f1e7;
      --panel: #fffaf2;
      --ink: #1f2933;
      --accent: #b34700;
      --accent-2: #ffcf99;
      --line: #dfd0bd;
      --ok: #1a7f37;
      --warn: #9a3412;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Avenir Next", "Trebuchet MS", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top right, rgba(255, 207, 153, 0.8), transparent 28%),
        linear-gradient(135deg, #fdf7ed 0%, var(--bg) 100%);
      min-height: 100vh;
    }
    .wrap {
      width: min(960px, calc(100% - 24px));
      margin: 0 auto;
      padding: 18px 0 28px;
    }
    .hero {
      margin-bottom: 14px;
      padding: 0;
      border: 1px solid var(--line);
      border-radius: 22px;
      background: linear-gradient(160deg, rgba(255,255,255,0.95), rgba(255,247,237,0.92));
      box-shadow: 0 20px 60px rgba(92, 53, 23, 0.08);
      overflow: hidden;
    }
    h1 {
      margin: 0;
      font-size: clamp(2rem, 4vw, 3rem);
      line-height: 0.95;
      letter-spacing: -0.04em;
    }
    .hero-logo {
      display: block;
      width: 100%;
      height: auto;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 14px;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 20px;
      padding: 16px;
      box-shadow: 0 10px 30px rgba(92, 53, 23, 0.06);
    }
    .card h2 {
      margin: 0 0 10px;
      font-size: 1.1rem;
    }
    label {
      display: block;
      margin: 0 0 5px;
      font-size: 0.88rem;
      font-weight: 600;
    }
    input {
      width: 100%;
      min-width: 0;
      padding: 10px 12px;
      border: 1px solid #cbb79f;
      border-radius: 12px;
      background: #fff;
      font: inherit;
    }
    .row, .field-pair, .triple-row {
      display: grid;
      gap: 10px;
      align-items: end;
    }
    .row, .field-pair {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
    .field-row {
      display: flex;
      align-items: center;
      justify-content: flex-start;
      gap: 6px;
      flex-wrap: nowrap;
      width: 100%;
    }
    .field-row button {
      height: 42px;
      padding: 0 16px;
      white-space: nowrap;
      min-width: 0;
    }
    .field-row.fill-button button {
      flex: 1 1 0;
    }
    .triple-row {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .field {
      min-width: 0;
    }
    .field.compact {
      width: 5.5ch;
    }
    .field.tiny {
      width: 6ch;
    }
    .field.medium {
      width: 7.5ch;
    }
    .field.wide {
      width: 7ch;
    }
    .field.compact input {
      width: 5.5ch;
      max-width: 5.5ch;
    }
    .field.tiny input {
      width: 6ch;
      max-width: 6ch;
    }
    .field.medium input {
      width: 7.5ch;
      max-width: 7.5ch;
    }
    .field.wide input {
      width: 7ch;
      max-width: 7ch;
    }
    .action-row,
    .control-row {
      display: flex;
      align-items: flex-end;
      width: 100%;
      flex-wrap: nowrap;
    }
    .action-row {
      gap: 6px;
    }
    .control-row {
      gap: 6px;
    }
    .action-row .field,
    .control-row .field {
      flex: 0 0 auto;
    }
    .action-row button,
    .control-row button {
      height: 42px;
      padding: 0 16px;
      white-space: nowrap;
      flex: 1 1 0;
      min-width: 0;
    }
    .compact-actions button {
      flex: 0 0 auto;
    }
    .action-row.fill-button button,
    .control-row.fill-button button {
      flex: 1 1 0;
    }
    .control-row.compact-actions {
      gap: 2px;
    }
    .field-pair > .compact-actions {
      min-width: 0;
    }
    .field-pair > .compact-actions .inline-field {
      flex-wrap: nowrap;
      min-width: 0;
    }
    .field-pair > .compact-actions .inline-label {
      white-space: nowrap;
    }
    .stack {
      display: grid;
      gap: 10px;
    }
    .subhead {
      margin: 4px 0 2px;
      font-size: 1.1rem;
      font-weight: 800;
      letter-spacing: 0;
      color: var(--ink);
    }
    .actions, .mini-actions {
      display: flex;
      flex-wrap: nowrap;
      gap: 10px;
      margin-top: 12px;
    }
    .actions button, .mini-actions button {
      flex: 1 1 0;
      min-width: 0;
      white-space: nowrap;
    }
    button {
      border: 0;
      border-radius: 999px;
      padding: 11px 14px;
      font: inherit;
      font-weight: 700;
      cursor: pointer;
      background: #1f2933;
      color: #fff;
    }
    button.primary { background: var(--accent); }
    button.secondary { background: #415a77; }
    button.success { background: #1a7f37; }
    button.danger { background: #b42318; }
    button.toggle-on { background: #1a7f37; }
    button.toggle-off { background: #b42318; }
    .status {
      display: grid;
      gap: 8px;
      margin-top: 10px;
    }
    .status-top {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr) auto;
      gap: 10px;
      align-items: center;
    }
    .pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      width: fit-content;
      padding: 8px 12px;
      border-radius: 999px;
      font-weight: 700;
      background: #efe4d6;
    }
    #statusText.status-ready {
      color: #1a7f37;
    }
    #statusText.status-busy {
      color: #b42318;
    }
    .meta {
      color: #52606d;
      font-size: 0.95rem;
    }
    .hint {
      margin-top: 8px;
      font-size: 0.84rem;
      color: #6b7280;
    }
    .progress {
      width: 100%;
      height: 14px;
      border-radius: 999px;
      background: #eadcc8;
      overflow: hidden;
    }
    .progress-bar {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, var(--accent), #e67e22);
      transition: width 0.25s ease;
    }
    .checkline {
      display: flex;
      align-items: center;
      gap: 8px;
      margin: 0;
      font-size: 0.86rem;
      font-weight: 600;
      white-space: nowrap;
      align-self: center;
      transform: none;
    }
    .checkline.right {
      margin-left: auto;
      justify-content: flex-end;
    }
    .checkline input {
      width: auto;
      margin: 0;
      accent-color: var(--accent);
      transform: scale(1.25);
      transform-origin: center;
    }
    .inline-label {
      margin: 0;
      white-space: nowrap;
      flex: 0 0 auto;
    }
    .inline-field {
      display: flex;
      align-items: center;
      gap: 6px;
      flex-wrap: nowrap;
    }
    .inline-field.push-right {
      margin-left: auto;
      justify-content: flex-end;
    }
    .slider-field {
      display: flex;
      align-items: center;
      gap: 10px;
      width: 100%;
      min-width: 0;
      justify-content: flex-end;
      margin-left: auto;
    }
    .slider-field input[type="range"] {
      width: 100%;
      min-width: 0;
      padding: 0;
      border: 0;
      background: transparent;
      accent-color: var(--accent);
      -webkit-appearance: none;
      appearance: none;
    }
    .slider-field input[type="range"]::-webkit-slider-runnable-track {
      height: 10px;
      border-radius: 999px;
      background: #dec9ac;
    }
    .slider-field input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 28px;
      height: 28px;
      border-radius: 50%;
      background: var(--accent);
      border: 3px solid #fffaf2;
      box-shadow: 0 2px 6px rgba(31, 41, 51, 0.18);
      margin-top: -9px;
      cursor: pointer;
    }
    .slider-field input[type="range"]::-moz-range-track {
      height: 10px;
      border-radius: 999px;
      background: #dec9ac;
    }
    .slider-field input[type="range"]::-moz-range-thumb {
      width: 28px;
      height: 28px;
      border-radius: 50%;
      background: var(--accent);
      border: 3px solid #fffaf2;
      box-shadow: 0 2px 6px rgba(31, 41, 51, 0.18);
      cursor: pointer;
    }
    .slider-value {
      min-width: 3.8ch;
      text-align: right;
      font-weight: 700;
      white-space: nowrap;
    }
    .inline-field.slider-inline {
      width: 100%;
      justify-content: space-between;
    }
    .inline-field.slider-inline .inline-label {
      flex: 0 0 auto;
    }
    .bottom-card {
      margin-top: 14px;
    }
    .inline-field .field {
      flex: 0 0 auto;
    }
    @media (max-width: 700px) {
      .actions, .mini-actions, .action-row, .control-row {
        flex-wrap: wrap;
      }
      .actions button, .mini-actions button {
        flex: 1 1 calc(50% - 10px);
      }
      .field-pair, .triple-row {
        grid-template-columns: 1fr;
      }
      .field-row {
        flex-wrap: nowrap;
      }
      .field.compact input,
      .field.tiny input,
      .field.medium input {
        width: auto;
        max-width: 100%;
      }
      .field.compact,
      .field.tiny,
      .field.medium {
        width: auto;
      }
    }
    @media (min-width: 701px) and (max-width: 1099px) {
      .wrap {
        width: min(100%, calc(100% - 12px));
        padding: 8px 0 14px;
      }
      .hero {
        margin-bottom: 10px;
        padding: 0;
        border-radius: 16px;
      }
      h1 {
        font-size: clamp(1.9rem, 5vw, 2.8rem);
      }
      .grid {
        grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
        gap: 10px;
      }
      .card {
        padding: 12px 14px 14px;
        border-radius: 16px;
      }
      .card h2 {
        margin-bottom: 10px;
        font-size: 1rem;
      }
      label,
      .inline-label,
      .checkline {
        font-size: 0.8rem;
      }
      input {
        padding: 8px 10px;
        border-radius: 10px;
      }
      button {
        padding: 9px 12px;
        font-size: 0.92rem;
      }
      .action-row,
      .control-row,
      .field-row,
      .inline-field {
        gap: 6px;
      }
      .action-row button,
      .control-row button,
      .actions button,
      .mini-actions button,
      .field-row button {
        min-height: 40px;
      }
      .field.compact {
        width: 4.5ch;
      }
      .field.compact input {
        width: 4.5ch;
        max-width: 4.5ch;
      }
      .field.tiny {
        width: 5.5ch;
      }
      .field.tiny input {
        width: 5.5ch;
        max-width: 5.5ch;
      }
      .field.medium {
        width: 6.5ch;
      }
      .field.medium input {
        width: 6.5ch;
        max-width: 6.5ch;
      }
      .field.wide {
        width: 6ch;
      }
      .field.wide input {
        width: 6ch;
        max-width: 6ch;
      }
      .pill {
        padding: 6px 10px;
      }
      .status-top {
        gap: 8px;
      }
      .progress {
        height: 12px;
      }
      .actions,
      .mini-actions {
        gap: 8px;
        margin-top: 10px;
      }
      .checkline input {
        transform: scale(1.1);
      }
    }
    @media (min-width: 1100px) {
      .wrap {
        width: min(1380px, calc(100% - 56px));
        padding: 28px 0 40px;
      }
      .hero {
        margin-bottom: 24px;
        padding: 0;
        border-radius: 28px;
      }
      h1 {
        font-size: clamp(3.2rem, 5vw, 4.6rem);
      }
      .grid {
        grid-template-columns: minmax(420px, 0.92fr) minmax(620px, 1.08fr);
        gap: 24px;
        align-items: start;
      }
      .card {
        padding: 24px 28px 28px;
        border-radius: 28px;
      }
      .card h2 {
        margin-bottom: 18px;
        font-size: 1.55rem;
      }
      label,
      .inline-label,
      .checkline {
        font-size: 1rem;
      }
      .action-row,
      .control-row,
      .field-row,
      .inline-field {
        gap: 12px;
      }
      .field-pair {
        grid-template-columns: minmax(240px, auto) minmax(0, 1fr);
        gap: 16px;
        align-items: center;
      }
      .field.medium {
        width: 8.5ch;
      }
      .field.medium input {
        width: 8.5ch;
        max-width: 8.5ch;
      }
      .control-row .field.tiny {
        width: 8.5ch;
      }
      .control-row .field.tiny input {
        width: 8.5ch;
        max-width: 8.5ch;
      }
      .field.wide {
        width: 8.5ch;
      }
      .field.wide input {
        width: 8.5ch;
        max-width: 8.5ch;
      }
      .action-row button,
      .control-row button,
      .actions button,
      .mini-actions button,
      .field-row button {
        min-height: 52px;
      }
      .action-row button {
        flex: 1 1 300px;
      }
      .control-row.fill-button button {
        flex: 1 1 320px;
      }
      .field-row.fill-button button {
        flex: 1 1 280px;
      }
      .status {
        margin-top: 18px;
      }
      .status-top {
        grid-template-columns: auto minmax(320px, 1fr) auto;
        gap: 14px;
      }
      .actions,
      .mini-actions {
        gap: 16px;
        margin-top: 18px;
      }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <section class="hero">
      <img class="hero-logo" src="/logo.jpg" alt="KOZOcut">
    </section>

    <section class="grid">
      <div class="card">
        <h2>Produkcja</h2>
        <div class="action-row">
          <div class="inline-field">
            <label class="inline-label" for="pieces">Ilość kabli</label>
            <div class="field compact">
              <input id="pieces" type="number" min="1" max="999" step="1" data-max-chars="3">
            </div>
          </div>
          <div class="inline-field push-right">
            <label class="inline-label" for="lengthMm">Długość kabla [mm]</label>
            <div class="field medium">
              <input id="lengthMm" type="number" min="0.1" max="999.9" step="0.1" data-max-chars="5">
            </div>
          </div>
        </div>

        <div class="status">
          <div class="status-top">
            <div class="pill">Stan: <span id="statusText">Gotowy</span></div>
            <div class="progress">
              <div class="progress-bar" id="progressBar"></div>
            </div>
            <div class="meta"><span id="progressText">0/0</span></div>
          </div>
        </div>

        <div class="actions">
          <button id="productionActionButton" class="success" onclick="productionAction()">Start</button>
          <button id="pauseButton" class="secondary" onclick="togglePause()">Pauza</button>
        </div>
      </div>

      <div class="card">
        <div class="stack">
          <div class="subhead">Kalibracja ekstrudera</div>
          <div class="field-row">
            <label class="inline-label" for="stepsPerMm">Kroki ekstrudera</label>
            <div class="field medium">
              <input id="stepsPerMm" type="number" min="0.001" max="999.999" step="0.001" data-max-chars="7">
            </div>
            <button class="primary" onclick="jogFeed(50)">50</button>
            <label class="checkline right" for="extruderInvertDir">
              <input id="extruderInvertDir" type="checkbox">
              Odwróć
            </label>
          </div>

          <div class="field-pair">
            <div class="inline-field slider-inline">
              <label class="inline-label" for="feedSpeedPercent">Szybkość podawania</label>
              <div class="slider-field">
                <input id="feedSpeedPercent" type="range" min="1" max="100" step="1">
                <span class="slider-value" id="feedSpeedPercentValue">85%</span>
              </div>
            </div>
            <div class="action-row compact-actions fill-button">
              <button id="testExtruderButton" class="success" onclick="testExtruder()">Test ekstrudera</button>
            </div>
          </div>
        </div>

        <div class="stack" style="margin-top: 12px;">
          <div class="subhead">Kalibracja gilotyny</div>
          <div class="field-row fill-button">
            <label class="inline-label" for="guillotineStepsPerCut">Kroki gilotyny</label>
            <div class="field wide">
              <input id="guillotineStepsPerCut" type="number" min="1" max="9999" step="1" data-max-chars="4">
            </div>
            <button class="primary" onclick="setGuillotine()">100</button>
            <label class="checkline right" for="guillotineInvertDir">
              <input id="guillotineInvertDir" type="checkbox">
              Odwróć
            </label>
          </div>

          <div class="control-row compact-actions">
            <div class="inline-field slider-inline">
              <label class="inline-label" for="cutSpeedPercent">Szybkość gilotyny</label>
              <div class="slider-field">
                <input id="cutSpeedPercent" type="range" min="1" max="100" step="1">
                <span class="slider-value" id="cutSpeedPercentValue">80%</span>
              </div>
            </div>
          </div>

          <div class="actions" style="margin-top: 8px;">
            <button id="testCutButton" class="success" onclick="testCut()">Test gilotyny</button>
          </div>
        </div>

        <div class="mini-actions">
          <button id="persistButton" class="secondary" onclick="persistConfig()">Zapisz ustawienia</button>
          <button class="secondary" onclick="resetConfig()">Resetuj ustawienia</button>
        </div>
      </div>

    </section>

    <section class="card bottom-card">
      <div class="subhead">Stripper</div>
      <div class="field-row fill-button">
        <button id="stripperToggleButton" class="danger" onclick="toggleStripper()">Off</button>
        <label class="checkline right" for="stripperInvertDir">
          <input id="stripperInvertDir" class="stripper-control" type="checkbox">
          Odwróć
        </label>
      </div>
      <div class="control-row compact-actions" style="margin-top: 10px;">
        <div class="inline-field slider-inline">
          <label class="inline-label" for="stripperSpeedPercent">Prędkość</label>
          <div class="slider-field">
            <input id="stripperSpeedPercent" class="stripper-control" type="range" min="0" max="100" step="1">
            <span class="slider-value" id="stripperSpeedPercentValue">0%</span>
          </div>
        </div>
      </div>
    </section>
  </div>

  <script>
    let isEditing = false;
    let hasUnsavedChanges = false;
    let lastSavedSnapshot = null;
    let stripperEnabledState = false;
    let stripperPausedState = false;

    function statusLabel(status) {
      if (status === "idle") return "Gotowy";
      if (status === "paused") return "Pauza";
      if (status === "feeding" || status === "jog") return "Ekstrud";
      if (status === "cutting") return "Cut";
      if (status === "stopped") return "Stop";
      if (status === "running") return "Praca";
      return status;
    }

    async function api(path, options = {}) {
      const response = await fetch(path, options);
      if (!response.ok) throw new Error("HTTP " + response.status);
      const type = response.headers.get("content-type") || "";
      return type.includes("application/json") ? response.json() : response.text();
    }

    function setValue(id, value) {
      document.getElementById(id).value = value;
      updateSliderValue(id);
    }

    function getNumber(id) {
      return Number(document.getElementById(id).value);
    }

    function updateSliderValue(id) {
      const valueEl = document.getElementById(id + "Value");
      if (!valueEl) return;
      valueEl.textContent = document.getElementById(id).value + "%";
    }

    function nextPaint() {
      return new Promise((resolve) => requestAnimationFrame(() => resolve()));
    }

    function updateStripperButton() {
      const button = document.getElementById("stripperToggleButton");
      const running = stripperEnabledState && !stripperPausedState;
      const paused = stripperEnabledState && stripperPausedState;
      button.textContent = running ? "Stop" : "Start";
      button.classList.toggle("danger", running);
      button.classList.toggle("toggle-off", running);
      button.classList.toggle("success", !running);
      button.classList.toggle("toggle-on", !running || paused);
    }

    function setButtonActive(id, active, idleLabel, activeLabel) {
      const button = document.getElementById(id);
      button.textContent = active ? activeLabel : idleLabel;
      button.classList.toggle("danger", active);
      button.classList.toggle("success", !active);
    }

    function updateProductionButtons(data) {
      const productionBusy = Boolean(data.running);
      const paused = data.status === "paused";
      setButtonActive("productionActionButton", productionBusy, "Start", "Stop");
      const pauseButton = document.getElementById("pauseButton");
      pauseButton.textContent = paused ? "Wznów" : "Pauza";
      pauseButton.disabled = !productionBusy && !paused;
    }

    function currentFormSnapshot() {
      return {
        pieces: getNumber("pieces"),
        lengthMm: getNumber("lengthMm"),
        stepsPerMm: getNumber("stepsPerMm"),
        feedSpeedPercent: getNumber("feedSpeedPercent"),
        cutSpeedPercent: getNumber("cutSpeedPercent"),
        stripperSpeedPercent: getNumber("stripperSpeedPercent"),
        guillotineStepsPerCut: getNumber("guillotineStepsPerCut"),
        extruderInvertDir: document.getElementById("extruderInvertDir").checked,
        guillotineInvertDir: document.getElementById("guillotineInvertDir").checked
      };
    }

    function savedSnapshotFromStatus(data) {
      return {
        pieces: Number(data.savedPieces),
        lengthMm: Number(data.savedLengthMm),
        stepsPerMm: Number(data.savedStepsPerMm),
        feedSpeedPercent: Number(data.savedFeedSpeedPercent),
        cutSpeedPercent: Number(data.savedCutSpeedPercent),
        stripperSpeedPercent: Number(data.savedStripperSpeedPercent),
        guillotineStepsPerCut: Number(data.savedGuillotineStepsPerCut),
        extruderInvertDir: Boolean(data.savedExtruderInvertDir),
        guillotineInvertDir: Boolean(data.savedGuillotineInvertDir)
      };
    }

    function snapshotsMatch(a, b) {
      if (!a || !b) return false;
      const eps = 0.0001;
      return (
        Math.abs(a.pieces - b.pieces) < eps &&
        Math.abs(a.lengthMm - b.lengthMm) < eps &&
        Math.abs(a.stepsPerMm - b.stepsPerMm) < eps &&
        Math.abs(a.feedSpeedPercent - b.feedSpeedPercent) < eps &&
        Math.abs(a.cutSpeedPercent - b.cutSpeedPercent) < eps &&
        Math.abs(a.stripperSpeedPercent - b.stripperSpeedPercent) < eps &&
        Math.abs(a.guillotineStepsPerCut - b.guillotineStepsPerCut) < eps &&
        a.extruderInvertDir === b.extruderInvertDir &&
        a.guillotineInvertDir === b.guillotineInvertDir
      );
    }

    function updatePersistButtonState() {
      const button = document.getElementById("persistButton");
      const matchesSaved = snapshotsMatch(currentFormSnapshot(), lastSavedSnapshot);
      button.classList.toggle("danger", !matchesSaved);
      button.classList.toggle("secondary", matchesSaved);
    }

    async function loadStatus() {
      const data = await api("/api/status");
      const completed = Number(data.completedPieces || 0);
      const target = Number(data.pieces || 0);
      const percent = target > 0 ? Math.max(0, Math.min(100, (completed * 100) / target)) : 0;
      lastSavedSnapshot = savedSnapshotFromStatus(data);

      const statusText = document.getElementById("statusText");
      statusText.textContent = statusLabel(data.status);
      const isReady = data.status === "idle";
      statusText.classList.toggle("status-ready", isReady);
      statusText.classList.toggle("status-busy", !isReady);
      document.getElementById("progressText").textContent = completed + "/" + target;
      document.getElementById("progressBar").style.width = percent + "%";

      if (!isEditing && !hasUnsavedChanges) {
        setValue("pieces", data.pieces);
        setValue("lengthMm", data.lengthMm);
        setValue("stepsPerMm", data.stepsPerMm);
        setValue("feedSpeedPercent", data.feedSpeedPercent);
        setValue("cutSpeedPercent", data.cutSpeedPercent);
        setValue("guillotineStepsPerCut", data.guillotineStepsPerCut);
        document.getElementById("extruderInvertDir").checked = Boolean(data.extruderInvertDir);
        document.getElementById("guillotineInvertDir").checked = Boolean(data.guillotineInvertDir);
      }
      stripperEnabledState = Boolean(data.stripperEnabled);
      stripperPausedState = Boolean(data.stripperEnabled) && data.status === "paused";
      updateStripperButton();
      document.getElementById("stripperInvertDir").checked = Boolean(data.stripperInvertDir);
      setValue("stripperSpeedPercent", data.stripperSpeedPercent);
      updateProductionButtons(data);
      setButtonActive("testExtruderButton", data.status === "jog", "Test ekstrudera", "Test ekstrudera");
      setButtonActive("testCutButton", data.status === "cutting", "Test gilotyny", "Test gilotyny");
      updatePersistButtonState();
    }

    function configFormData() {
      const snapshot = currentFormSnapshot();
      return new URLSearchParams({
        pieces: snapshot.pieces,
        lengthMm: snapshot.lengthMm,
        stepsPerMm: snapshot.stepsPerMm,
        feedSpeedPercent: snapshot.feedSpeedPercent,
        cutSpeedPercent: snapshot.cutSpeedPercent,
        guillotineStepsPerCut: snapshot.guillotineStepsPerCut,
        extruderInvertDir: snapshot.extruderInvertDir ? "1" : "0",
        guillotineInvertDir: snapshot.guillotineInvertDir ? "1" : "0"
      });
    }

    async function saveConfig(refresh = true) {
      isEditing = false;
      hasUnsavedChanges = false;
      await api("/api/config", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
        body: configFormData().toString()
      });
      if (refresh) {
        await loadStatus();
      }
    }

    async function persistConfig() {
      await saveConfig(false);
      await updateStripper();
      await api("/api/save", { method: "POST" });
      await loadStatus();
    }

    async function resetConfig() {
      isEditing = false;
      hasUnsavedChanges = false;
      await api("/api/reset", { method: "POST" });
      await loadStatus();
    }

    async function productionAction() {
      const actionButton = document.getElementById("productionActionButton");
      if (actionButton.textContent === "Stop") {
        await api("/api/stop", { method: "POST" });
        await loadStatus();
        return;
      }
      await saveConfig(false);
      await api("/api/start", { method: "POST" });
      await loadStatus();
    }

    async function togglePause() {
      await api("/api/pause", { method: "POST" });
      await loadStatus();
    }

    async function unloadExtruder() {
      await saveConfig(false);
      await api("/api/unload", { method: "POST" });
      await loadStatus();
    }

    async function jogFeed(mm) {
      await saveConfig(false);
      await api("/api/jog?mm=" + encodeURIComponent(mm), { method: "POST" });
      await loadStatus();
    }

    async function testExtruder() {
      setButtonActive("testExtruderButton", true, "Test ekstrudera", "Test ekstrudera");
      await nextPaint();
      await jogFeed(getNumber("lengthMm"));
      setButtonActive("testExtruderButton", false, "Test ekstrudera", "Test ekstrudera");
    }

    async function testCut() {
      setButtonActive("testCutButton", true, "Test gilotyny", "Test gilotyny");
      await nextPaint();
      await saveConfig(false);
      await api("/api/cut", { method: "POST" });
      await loadStatus();
      setButtonActive("testCutButton", false, "Test gilotyny", "Test gilotyny");
    }

    async function setGuillotine() {
      await saveConfig(false);
      await api("/api/guillotine-set", { method: "POST" });
      await loadStatus();
    }

    async function updateStripper() {
      await api("/api/stripper", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
        body: new URLSearchParams({
          enabled: stripperEnabledState ? "1" : "0",
          invertDir: document.getElementById("stripperInvertDir").checked ? "1" : "0",
          speedPercent: getNumber("stripperSpeedPercent")
        }).toString()
      });
      await loadStatus();
    }

    async function toggleStripper() {
      if (stripperEnabledState && stripperPausedState) {
        await api("/api/pause", { method: "POST" });
        await loadStatus();
        return;
      }
      stripperEnabledState = !stripperEnabledState;
      stripperPausedState = false;
      updateStripperButton();
      await updateStripper();
    }

    for (const input of document.querySelectorAll("input:not(.stripper-control)")) {
      input.addEventListener("focus", () => { isEditing = true; });
      input.addEventListener("input", () => {
        const maxChars = Number(input.dataset.maxChars || 0);
        if (input.type !== "range" && maxChars > 0 && input.value.length > maxChars) {
          input.value = input.value.slice(0, maxChars);
        }
        updateSliderValue(input.id);
        isEditing = true;
        hasUnsavedChanges = true;
        updatePersistButtonState();
      });
      input.addEventListener("change", () => {
        hasUnsavedChanges = true;
        updatePersistButtonState();
      });
      input.addEventListener("blur", () => {
        isEditing = false;
        const active = document.activeElement;
        if (active && active.tagName === "INPUT") {
          isEditing = true;
        }
      });
    }

    document.getElementById("stripperInvertDir").addEventListener("change", updateStripper);
    document.getElementById("stripperSpeedPercent").addEventListener("input", async () => {
      updateSliderValue("stripperSpeedPercent");
      await updateStripper();
    });

    updateSliderValue("feedSpeedPercent");
    updateSliderValue("cutSpeedPercent");
    updateSliderValue("stripperSpeedPercent");
    updateStripperButton();

    loadStatus();
    setInterval(loadStatus, 1200);
  </script>
</body>
</html>
)HTML";

  server.send_P(200, "text/html; charset=utf-8", page);
}

void handleLogo() {
  server.send_P(200, PSTR("image/jpeg"),
                reinterpret_cast<PGM_P>(logotyp_web_jpg),
                logotyp_web_jpg_len);
}

void handleConfigUpdate() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }

  if (server.hasArg("pieces")) {
    config.pieces = max<uint32_t>(1U, static_cast<uint32_t>(server.arg("pieces").toInt()));
  }
  if (server.hasArg("lengthMm")) {
    config.lengthMm = max(0.1f, server.arg("lengthMm").toFloat());
  }
  if (server.hasArg("stepsPerMm")) {
    config.stepsPerMm = max(0.001f, server.arg("stepsPerMm").toFloat());
  }
  if (server.hasArg("feedSpeedPercent")) {
    config.feedSpeedPercent = clampPercent(static_cast<uint32_t>(
        max(1L, server.arg("feedSpeedPercent").toInt())));
  }
  if (server.hasArg("cutSpeedPercent")) {
    config.cutSpeedPercent = clampPercent(static_cast<uint32_t>(
        max(1L, server.arg("cutSpeedPercent").toInt())));
  }
  if (server.hasArg("guillotineStepsPerCut")) {
    config.guillotineStepsPerCut =
        max<uint32_t>(1U, static_cast<uint32_t>(server.arg("guillotineStepsPerCut").toInt()));
  }
  config.extruderInvertDir =
      parseBooleanArg("extruderInvertDir", config.extruderInvertDir);
  config.guillotineInvertDir =
      parseBooleanArg("guillotineInvertDir", config.guillotineInvertDir);

  sendJsonStatus();
}

void handleStart() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is already running");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  runProductionCycle();
}

void handleStop() {
  state.stopRequested = true;
  sendJsonStatus();
}

void handlePause() {
  if (!togglePauseState()) {
    server.send(409, "text/plain", "Machine is not running");
    return;
  }
  sendJsonStatus();
}

void handleSavePersistent() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  saveConfigToNvs();
  sendJsonStatus();
}

void handleResetConfig() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  loadConfigFromNvs();
  sendJsonStatus();
}

void handleStripper() {
  if (isMotorActive()) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }

  stripper.enabled = parseBooleanArg("enabled", stripper.enabled);
  stripper.invertDir = parseBooleanArg("invertDir", stripper.invertDir);
  if (server.hasArg("speedPercent")) {
    stripper.speedPercent = min<uint32_t>(
        100U, static_cast<uint32_t>(max(0L, server.arg("speedPercent").toInt())));
  }

  if (!stripper.enabled) {
    disableStripper();
    if (state.paused) {
      state.paused = false;
      state.status = "idle";
    }
  } else {
    stripper.lastToggleUs = 0;
    stripper.stepHigh = false;
    if (state.paused) {
      state.paused = false;
      state.status = "idle";
    }
  }

  sendJsonStatus();
}

void handleJog() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  disableStripper();
  const float mm = server.hasArg("mm") ? server.arg("mm").toFloat() : 10.0f;
  setDriversEnabled(true);
  state.status = "jog";
  state.stopRequested = false;
  feedCableMm(fabsf(mm), mm >= 0.0f);
  setDriversEnabled(false);
  state.status = "idle";
  sendJsonStatus();
}

void handleCut() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  disableStripper();
  setDriversEnabled(true);
  state.status = "cutting";
  state.stopRequested = false;
  runCutCycle();
  setDriversEnabled(false);
  state.status = "idle";
  sendJsonStatus();
}

void handleGuillotineSet() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  disableStripper();
  setDriversEnabled(true);
  state.status = "cutting";
  state.stopRequested = false;
  moveGuillotineSteps(100, true);
  setDriversEnabled(false);
  state.status = "idle";
  sendJsonStatus();
}

void handleUnload() {
  if (state.running) {
    server.send(409, "text/plain", "Machine is running");
    return;
  }
  disableStripper();
  setDriversEnabled(true);
  state.status = "jog";
  state.stopRequested = false;
  feedCableMm(config.lengthMm, false);
  setDriversEnabled(false);
  state.status = "idle";
  sendJsonStatus();
}

void handleNotFound() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
  server.send(302, "text/plain", "");
}

void setupWifiAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kApIp, kApGateway, kApSubnet);
  WiFi.softAP(kApSsid);
  dnsServer.start(kDnsPort, "*", kApIp);
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/logo.jpg", HTTP_GET, handleLogo);
  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.on("/connecttest.txt", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, sendJsonStatus);
  server.on("/api/config", HTTP_POST, handleConfigUpdate);
  server.on("/api/save", HTTP_POST, handleSavePersistent);
  server.on("/api/start", HTTP_POST, handleStart);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/pause", HTTP_POST, handlePause);
  server.on("/api/stripper", HTTP_POST, handleStripper);
  server.on("/api/jog", HTTP_POST, handleJog);
  server.on("/api/cut", HTTP_POST, handleCut);
  server.on("/api/guillotine-set", HTTP_POST, handleGuillotineSet);
  server.on("/api/unload", HTTP_POST, handleUnload);
  server.on("/api/reset", HTTP_POST, handleResetConfig);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setupPins() {
  pinMode(Pins::EXTRUDER_DIR, OUTPUT);
  pinMode(Pins::EXTRUDER_STEP, OUTPUT);
  pinMode(Pins::GUILLOTINE_DIR, OUTPUT);
  pinMode(Pins::GUILLOTINE_STEP, OUTPUT);
  pinMode(Pins::DRIVER_ENABLE, OUTPUT);
  pinMode(Pins::BLUE_STATUS_LED, OUTPUT);
  pinMode(Pins::BOOT_BUTTON, INPUT_PULLUP);

  digitalWrite(Pins::EXTRUDER_STEP, LOW);
  digitalWrite(Pins::GUILLOTINE_STEP, LOW);
  setDriversEnabled(false);
  setBlueLed(false);
  lastBootButtonReading = digitalRead(Pins::BOOT_BUTTON);
  bootButtonStableState = lastBootButtonReading;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("BOOT early");
  delay(200);
  loadConfigFromNvs();
  setupPins();
  setupWifiAp();
  setupServer();
  updateStatusLed();

  Serial.println();
  Serial.println("KOZOcut ready");
  Serial.print("SSID: ");
  Serial.println(kApSsid);
  Serial.print("Panel: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  pollBootButton();
  serviceStripper();
  dnsServer.processNextRequest();
  server.handleClient();
  updateStatusLed();
  if (!(stripper.enabled && !isMotorActive() && !state.paused)) {
    delay(2);
  }
}
