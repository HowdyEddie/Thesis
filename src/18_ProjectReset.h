// =======================================================
// ProjectReset.ino
// One-call reset function for moving from test data to real project use.
// =======================================================

/*
  Function: resetEverythingForActualProject()
  Description:
    Clears the complete test setup so the locker can start as an actual project.

    This is the only reset function in this file.

    It clears:
      - ESP32 account Preferences
      - ESP32 key registration Preferences
      - ESP32 maintenance/slot Preferences
      - R503 fingerprint sensor templates
      - RAM accounts, keys, logs, date cards, and temporary values
      - slot states and output locks
      - known primary SD CSV files

    It does not reset the DS3231 RTC date/time.

  How to use:
    Add this once after runLoadingScreen() in setup():

      resetEverythingForActualProject();

    Upload once. After Serial Monitor shows reset complete,
    remove/comment the function call and upload again.
*/
void resetEverythingForActualProject() {
  debugLog("resetEverythingForActualProject", "Starting full actual-project reset.");

  qrScanActive = false;
  qrBuffer = "";
  lastQRByteTime = 0;
  pendingBorrowerName = "";
  pendingStudentCode = "";
  pendingQRRawData = "";
  pendingRegisterUserName = "";
  pendingFingerprintID = 0;
  pendingKeyName = "";
  pendingKeyNumber = "";
  pendingKeyRFID = "";
  selectedSlotIndex = -1;

  keyboardText = "";
  keyboardUpperMode = false;
  keyboardTarget = KB_TARGET_NONE;
  keyboardReturnPage = PAGE_LOCKED_SCREEN;

  adminInputValue = 0;
  adminInputText = "";
  adminInputReceived = false;
  savedAdminPIN = ADMIN_DEFAULT_PIN_TEXT;
  pendingOldAdminPIN = "";
  pendingNewAdminPIN = "";
  pendingConfirmAdminPIN = "";
  adminReturnPage = PAGE_LOCKED_SCREEN;

  clearCurrentHomeUser();

  usbInserted = false;
  previousUSBInserted = false;
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotUnlockActive[i] = false;
  }
  lastReturnedRFIDUID = "";
  lastRFIDReturnAction = 0;

  debugLog("resetEverythingForActualProject", "Clearing ESP32 Preferences.");
  prefs.begin("accounts", false);
  prefs.clear();
  prefs.end();

  prefs.begin("keys", false);
  prefs.clear();
  prefs.end();

  prefs.begin("locker", false);
  prefs.clear();
  prefs.end();

  if (!fpsReady) {
    debugLog("resetEverythingForActualProject", "FPS not ready. Trying to initialize before clearing templates.");
    initFingerprintModule();
  }

  if (fpsReady) {
    debugLog("resetEverythingForActualProject", "Clearing all fingerprint templates from R503 sensor.");

    uint8_t p = finger.emptyDatabase();

    if (p == FINGERPRINT_OK) {
      debugOK("resetEverythingForActualProject fingerprint emptyDatabase");
    } else {
      debugNumber("resetEverythingForActualProject", "finger.emptyDatabase error", p);
    }

    p = finger.getTemplateCount();

    if (p == FINGERPRINT_OK) {
      debugNumber("resetEverythingForActualProject", "Fingerprint templates after reset", finger.templateCount);
    } else {
      debugNumber("resetEverythingForActualProject", "getTemplateCount after reset error", p);
    }
  } else {
    debugFail("resetEverythingForActualProject", "FPS still not ready. Fingerprint database was not cleared.");
  }

  debugLog("resetEverythingForActualProject", "Removing archived log data from SD card.");

  if (SD.begin(SD_STORAGE_CS, SPI)) {
    const char *archiveDirectories[] = {"/daily", "/monthly"};
    for (uint8_t i = 0; i < 2; i++) {
      if (SD.exists(archiveDirectories[i]) &&
          !removeArchiveDirectoryTree(archiveDirectories[i])) {
        debugFail("resetEverythingForActualProject", "Could not remove an SD archive directory.");
      }
    }

    const char *filesToRemove[] = {
      "/accounts.csv",
      "/transactions.csv",
      "/keys.csv",
      "/returns.csv"
    };

    for (uint8_t i = 0; i < 4; i++) {
      if (SD.exists(filesToRemove[i])) {
        bool removed = SD.remove(filesToRemove[i]);
        debugValue("resetEverythingForActualProject", "Removed file", filesToRemove[i]);
        debugValue("resetEverythingForActualProject", "Remove result", removed ? "OK" : "FAILED");
      } else {
        debugValue("resetEverythingForActualProject", "File not found", filesToRemove[i]);
      }
    }

    ensureArchiveDirectory("/daily");
    ensureArchiveDirectory("/monthly");
    SD.end();
  } else {
    debugFail("resetEverythingForActualProject", "SD card not available at CS GPIO14. Archives were not removed.");
  }

  currentLogCount = 0;
  currentLogOffset = 0;
  activeCurrentLogCards = CURRENT_LOG_CARDS_6;

  transactionLogCount = 0;
  transactionLogOffset = 0;

  dateLogCount = 0;
  dateCardOffset = 0;
  selectedDateVisibleIndex = -1;
  selectedDateText = "";

  accountCount = 0;
  accountOffset = 0;

  for (uint8_t i = 0; i < MAX_CURRENT_LOGS; i++) {
    currentLogs[i].keyName = "";
    currentLogs[i].borrowerName = "";
    currentLogs[i].keyNumber = "";
    currentLogs[i].timeText = "";
    currentLogs[i].statusIcon = LOG_STATUS_EMPTY;
  }

  for (uint8_t i = 0; i < MAX_TRANSACTION_LOGS; i++) {
    transactionLogs[i].keyNo = "";
    transactionLogs[i].borrowerName = "";
    transactionLogs[i].keyName = "";
    transactionLogs[i].borrowedTime = "";
    transactionLogs[i].returnedTime = "";
    transactionLogs[i].userName = "";
    transactionLogs[i].dateText = "";
  }

  for (uint8_t i = 0; i < MAX_DATE_LOGS; i++) {
    dateLogs[i] = "";
  }

  for (uint8_t i = 0; i < MAX_ACCOUNTS; i++) {
    accounts[i].userNumber = "";
    accounts[i].userName = "";
    accounts[i].fingerprintID = 0;
    accounts[i].dateRegistered = "";
    accounts[i].used = false;
  }

  clearRegisteredKeysInRAM();

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotSelected[i] = false;
    slotTaken[i] = false;
    slotMaintenanceActive[i] = true;
    slotDisplayState[i] = SLOT_AVAILABLE;

    if (mcpReady) {
      setSlotLockOutput(i, false);
    }

    updateSlotDisplay(i);
  }

  selectedSlotIndex = -1;
  writeVPWord(VP_SELECTED_COUNT, 0);

  saveMaintenanceState();
  initDateLogs();

  clearHomeSlotKeyNames();

  renderCurrentLogs(CURRENT_LOG_CARDS_6);

  writeVPText(VP_ADMIN_LOGIN_STATUS_TEXT, "", 32);
  writeVPText(VP_REGISTER_NAME_TEXT, "", 32);
  writeVPText(VP_REGISTER_STATUS_TEXT, "Fresh reset done", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_IDLE);
  writeVPText(VP_KEYREG_KEY_NAME_TEXT, "", 32);
  writeVPText(VP_KEYREG_KEY_NO_TEXT, "", 16);
  writeVPText(VP_KEYREG_RFID_TEXT, "", KEYREG_RFID_BYTES);
  writeVPText(VP_KEYREG_STATUS_TEXT, "Fresh reset done", 32);
  writeVPText(VP_QR_NAME_TEXT, "", KEYBOARD_FIELD_BYTES);
  writeVPText(VP_QR_STATUS_TEXT, "Fresh reset done", 32);
  writeVPText(VP_QR_SELECTED_SLOT_TEXT, "", 16);
  writeVPText(VP_QR_STUDENT_CODE_TEXT, "", 24);

  goToLockedScreen();

  debugOK("resetEverythingForActualProject");
  debugLog("resetEverythingForActualProject", "Actual project reset complete. Remove/comment this function call and upload again.");
}
