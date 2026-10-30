// =======================================================
// Buzzer.h
// Non-blocking open-cover alert after the DWIN display sleeps.
// =======================================================

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_INACTIVE_LEVEL);
  buzzerOutputOn = false;
  buzzerCountdownRunning = false;
  buzzerPatternRunning = false;
}

void stopDoorCoverBuzzer() {
  if (buzzerOutputOn) {
    digitalWrite(BUZZER_PIN, BUZZER_INACTIVE_LEVEL);
    buzzerOutputOn = false;
  }
  buzzerPatternRunning = false;
}

void handleDoorCoverBuzzer() {
  bool limitSwitchActive =
      digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL;

  if (limitSwitchActive || !dwinBacklightSleeping) {
    buzzerCountdownStartedAt = millis();
    buzzerCountdownRunning = false;
    stopDoorCoverBuzzer();
    return;
  }

  if (!buzzerCountdownRunning) {
    buzzerCountdownStartedAt = millis();
    buzzerCountdownRunning = true;
    return;
  }

  if (!buzzerPatternRunning) {
    if (millis() - buzzerCountdownStartedAt < BUZZER_ALERT_DELAY_MS) return;
    buzzerPatternStartedAt = millis();
    buzzerPatternRunning = true;
  }

  unsigned long patternPosition =
      (millis() - buzzerPatternStartedAt) % BUZZER_PATTERN_CYCLE_MS;
  bool shouldBeep =
      patternPosition < 120 ||
      (patternPosition >= 250 && patternPosition < 370) ||
      (patternPosition >= 500 && patternPosition < 850);

  if (shouldBeep == buzzerOutputOn) return;

  digitalWrite(BUZZER_PIN,
               shouldBeep ? BUZZER_ACTIVE_LEVEL : BUZZER_INACTIVE_LEVEL);
  buzzerOutputOn = shouldBeep;
}
