// =======================================================
// DisplayIdle.ino
// ESP32-owned idle detection and DWIN backlight sleep/wake behavior.
// =======================================================

/*
  Function: initDisplayIdleTimer()
  Description:
    Starts the idle timer after the loading screen finishes.

  How to use:
    Called once in setup() after runLoadingScreen().
*/
void initDisplayIdleTimer() {
  lastUserActivityMillis = millis();
  dwinBacklightSleeping = false;
  wakeDWINBacklight();
  debugOK("initDisplayIdleTimer");
}

/*
  Function: registerUserActivity()
  Description:
    Marks that someone is currently using the locker. If the DWIN backlight
    is off, it wakes the display first.

  How to use:
    Call when there is a real user event: DWIN touch, fingerprint image,
    RFID tag, QR scan, or keyboard input.
*/
void registerUserActivity(const char *source) {
  lastUserActivityMillis = millis();

  if (dwinBacklightSleeping) {
    Serial.print("[registerUserActivity] Waking DWIN from: ");
    Serial.println(source);
    wakeDWINBacklight();
  }
}

/*
  Function: clearTemporarySessionForIdleSleep()
  Description:
    Clears only unfinished/in-progress screen data before the device returns
    to the Locked Screen due to inactivity. It does not erase accounts,
    registered keys, logs, CSV files, or fingerprint templates.

  How to use:
    Called by handleDisplayIdleSleep() right before going to Page 1.
*/
void clearTemporarySessionForIdleSleep() {
  qrScanActive = false;
  qrBuffer = "";
  pendingBorrowerName = "";
  pendingStudentCode = "";
  pendingQRRawData = "";
  selectedSlotIndex = -1;

  keyboardText = "";
  keyboardTarget = KB_TARGET_NONE;

  pendingOldAdminPIN = "";
  pendingNewAdminPIN = "";
  pendingConfirmAdminPIN = "";

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotSelected[i] = false;
  }

  debugLog("clearTemporarySessionForIdleSleep", "Temporary selected slot/QR/keyboard/update-password data cleared.");
}


/*
  Function: handleDisplayWakeTouch()
  Description:
    Handles the optional DWIN wake-touch VP. This VP is needed only if you
    want a blank area or dedicated touch object on the Locked Screen to wake
    the display. The ESP32 cannot detect a generic DWIN screen touch unless
    the DWIN object sends a Return Key Code / VP event through UART.

  How to use:
    Create a DWIN Return Key Code / Basic Touch object with:
      VP = 0x13F0
      Value = 0x0001
    Then call handleDisplayWakeTouch(vp, value) from handleDWIN().
*/
bool handleDisplayWakeTouch(uint16_t vp, uint16_t value) {
  if (vp != VP_DISPLAY_WAKE_TOUCH) return false;

  registerUserActivity("DWIN wake touch VP");
  writeVPWord(VP_DISPLAY_WAKE_TOUCH, 0);
  return true;
}

/*
  Function: handleDisplayIdleSleep()
  Description:
    Detects if no one is currently using the device. After the idle timeout,
    it cancels unfinished temporary actions, returns to the Locked Screen,
    renders the 6 Current Log cards, then turns the DWIN backlight off.

  How to use:
    Called continuously in loop().
*/
void handleDisplayIdleSleep() {
  if (dwinBacklightSleeping) return;
  if (currentPage == PAGE_LOADING) return;

  if (millis() - lastUserActivityMillis < DISPLAY_IDLE_TIMEOUT_MS) return;

  debugLog("handleDisplayIdleSleep", "Device idle. Returning to Locked Screen and turning off DWIN backlight.");

  clearTemporarySessionForIdleSleep();

  if (currentPage != PAGE_LOCKED_SCREEN) {
    goToLockedScreen();
  } else {
    renderCurrentLogs(CURRENT_LOG_CARDS_6);
    updateRTCDisplay();
  }

  sleepDWINBacklight();
}
