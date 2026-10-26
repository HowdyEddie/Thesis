// =======================================================
// Loading.ino
// Real loading screen with progress icon.
// =======================================================

const uint8_t MAX_STARTUP_FAILURES = 8;
const char *startupFailureModules[MAX_STARTUP_FAILURES];
uint8_t startupFailureCount = 0;

void reportStartupModuleStatus(const char *moduleName, bool ready) {
  Serial.print("[STARTUP] ");
  Serial.print(moduleName);
  Serial.println(ready ? ": READY" : ": FAILED");

  if (!ready && startupFailureCount < MAX_STARTUP_FAILURES) {
    startupFailureModules[startupFailureCount++] = moduleName;
  }
}

void reportStartupFailureSummary() {
  if (startupFailureCount == 0) {
    Serial.println("[STARTUP] All probed modules initialized.");
    return;
  }

  Serial.println("[STARTUP] Failed modules:");
  for (uint8_t i = 0; i < startupFailureCount; i++) {
    Serial.print("  - ");
    Serial.println(startupFailureModules[i]);
  }
  Serial.println("[STARTUP] Continuing to the Locked Screen in safe mode.");
}

/*
  Function: runLoadingScreen()
  Description:
    Attempts each module initialization, reports failures to Serial, loads
    persistent data where available, and always proceeds to the Locked Screen.

  How to use:
    Called once in setup().
*/
void runLoadingScreen() {
  debugLog("runLoadingScreen", "Starting hardware and data initialization.");
  startupFailureCount = 0;
  currentPage = PAGE_LOADING;
  changePage(PAGE_LOADING);
  delay(300);
  writeVPWord(VP_PROGRESS_ICON, 0);

  writeVPWord(VP_PROGRESS_ICON, 10);
  delay(100);

  debugLog("runLoadingScreen", "Reporting MCP23017 startup status...");
  reportStartupModuleStatus("MCP23017 slot expanders", mcpReady);
  writeVPWord(VP_PROGRESS_ICON, 20);
  delay(100);

  debugLog("runLoadingScreen", "Initializing RTC module...");
  reportStartupModuleStatus("DS3231 RTC", initRTCModule());
  writeVPWord(VP_PROGRESS_ICON, 30);
  delay(100);

  writeVPWord(VP_PROGRESS_ICON, 40);
  delay(100);

  debugLog("runLoadingScreen", "Initializing SD storage...");
  bool storageInitialized = initStorageModules();
  reportStartupModuleStatus("GPIO14 SD card", storageInitialized);
  writeVPWord(VP_PROGRESS_ICON, 55);
  delay(100);

  debugLog("runLoadingScreen", "Initializing CH376S USB host...");
  reportStartupModuleStatus("CH376S USB host (CS 25 / INT 35)", initUSBHost());
  writeVPWord(VP_PROGRESS_ICON, 65);
  delay(100);

  debugLog("runLoadingScreen", "Initializing RC522 RFID...");
  reportStartupModuleStatus("RC522 RFID reader", initRFIDModule());
  writeVPWord(VP_PROGRESS_ICON, 75);
  delay(100);

  debugLog("runLoadingScreen", "Initializing fingerprint sensor...");
  reportStartupModuleStatus("R503 fingerprint sensor", initFingerprintModule());
  writeVPWord(VP_PROGRESS_ICON, 85);
  delay(100);

  debugLog("runLoadingScreen", "Initializing QR scanner...");
  reportStartupModuleStatus("QR scanner serial interface", initQRScanner());
  Serial.println("[STARTUP] QR scanner presence is checked when scan data is received; manual name entry is available.");
  writeVPWord(VP_PROGRESS_ICON, 90);
  delay(100);

  debugLog("runLoadingScreen", "Loading saved maintenance states...");
  loadMaintenanceState();
  debugLog("runLoadingScreen", "Preparing current logs...");
  initCurrentLogs();
  debugLog("runLoadingScreen", "Preparing transaction logs...");
  initTransactionLogs();
  debugLog("runLoadingScreen", "Preparing date log cards...");
  initDateLogs();
  debugLog("runLoadingScreen", "Loading saved daily transaction history...");
  bool transactionHistoryLoaded = loadLogsFromSD();
  storageSDReady = storageInitialized && storageSDReady && transactionHistoryLoaded;
  reportStartupModuleStatus("SD archive/history readiness", storageSDReady);
  debugLog("runLoadingScreen", "Loading saved admin PIN...");
  loadAdminPINFromPreferences();

  debugLog("runLoadingScreen", "Loading saved accounts...");
  loadAccountsFromPreferences();
  debugLog("runLoadingScreen", "Loading registered keys...");
  loadKeysFromPreferences();

  writeVPWord(VP_PROGRESS_ICON, 99);
  delay(500);

  reportStartupFailureSummary();
  debugOK("runLoadingScreen");
  goToLockedScreen();
}
