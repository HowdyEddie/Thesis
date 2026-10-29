// =======================================================
// LimitSwitch.ino
// One safety limit switch: when HIGH, lock all slot outputs and return to Page 1.
// =======================================================

/*
  Function: initLimitSwitch()
  Description:
    Initializes the single limit switch input used as a safety lock trigger.
    The selected pin is GPIO34. GPIO34 is input-only and has no internal
    pull-up or pull-down resistor on ESP32, so use an external 10k pulldown
    resistor to GND if the switch connects the pin to 3.3V.

  How to use:
    Called once in setup() after initHardwarePins().
*/
void initLimitSwitch() {
  pinMode(LIMIT_SWITCH_PIN, INPUT);
  limitSwitchPreviouslyActive = false;
  lastLimitSwitchCheck = 0;

  debugNumber("initLimitSwitch", "Limit switch GPIO", LIMIT_SWITCH_PIN);
  debugOK("initLimitSwitch");
}

/*
  Function: lockAllSlotSolenoids()
  Description:
    Forces all 40 slot solenoid outputs to the inactive/locked level.
    This only locks the physical outputs. It does not change saved
    maintenance active/inactive settings and does not erase logs.

  How to use:
    Called automatically by handleLimitSwitchSafetyLock() when the
    limit switch reads HIGH.
*/
void lockAllSlotSolenoids() {
  debugLog("lockAllSlotSolenoids", "Locking all slot solenoid outputs.");

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    setSlotLockOutput(i, false);
  }

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotUnlockActive[i] = false;
  }

  debugOK("lockAllSlotSolenoids");
}

/*
  Function: handleLimitSwitchSafetyLock()
  Description:
    Checks the single limit switch. If it reads HIGH, all slot solenoids
    are forced inactive/locked, temporary borrow/QR selections are cleared,
    and the display returns to Page 1 Locked Screen.

    The action is edge-triggered, so it runs once when the switch first
    becomes HIGH and will not repeatedly reload Page 1 while the switch
    remains HIGH.

  How to use:
    Called continuously in loop().
*/
void handleLimitSwitchSafetyLock() {
  if (millis() - lastLimitSwitchCheck < LIMIT_SWITCH_CHECK_INTERVAL) return;
  lastLimitSwitchCheck = millis();

  bool limitActive = (digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL);

  if (limitActive && !limitSwitchPreviouslyActive) {
    debugLog("handleLimitSwitchSafetyLock", "Limit switch HIGH. Locking all slots and returning to Locked Screen.");

    lockAllSlotSolenoids();

    qrScanActive = false;
    qrBuffer = "";
    pendingBorrowerName = "";
    pendingStudentCode = "";
    pendingQRRawData = "";
    selectedSlotIndex = -1;

    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
      slotSelected[i] = false;
    }

    if (currentPage == PAGE_HOME_DASHBOARD_A ||
        currentPage == PAGE_HOME_DASHBOARD_B ||
        currentPage == PAGE_MAINTENANCE_A ||
        currentPage == PAGE_MAINTENANCE_B) {
      for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        updateSlotDisplay(i);
      }
    }

    if (currentPage != PAGE_LOCKED_SCREEN) {
      goToLockedScreen();
    } else {
      renderCurrentLogs(CURRENT_LOG_CARDS_6);
      updateRTCDisplay();
    }

    debugLog("handleLimitSwitchSafetyLock", "Reinitializing fingerprint sensor after safety lock event.");
    fpsReady = initFingerprintModule();
    lastFingerprintCheck = millis();
  }

  limitSwitchPreviouslyActive = limitActive;
}
