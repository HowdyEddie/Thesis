// =======================================================
// Loading.ino
// Real loading screen with progress icon.
// =======================================================

/*
  Function: runLoadingScreen()
  Description:
    Opens Page 0, initializes required modules, updates the progress icon,
    loads saved data, then goes to the locked screen.

  How to use:
    Called once in setup().
*/
void runLoadingScreen() {
  debugLog("runLoadingScreen", "Starting hardware and data initialization.");
  currentPage = PAGE_LOADING;
  changePage(PAGE_LOADING);
  delay(300);

  writeVPWord(VP_PROGRESS_ICON, 0);

  Wire.begin(I2C_SDA, I2C_SCL);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  writeVPWord(VP_PROGRESS_ICON, 10);
  delay(100);

  debugLog("runLoadingScreen", "Initializing RTC module...");
  initRTCModule();
  writeVPWord(VP_PROGRESS_ICON, 25);
  delay(100);

  debugLog("runLoadingScreen", "Initializing MCP23017 expanders...");
  initIOExpanders();
  writeVPWord(VP_PROGRESS_ICON, 40);
  delay(100);

  debugLog("runLoadingScreen", "Initializing SD storage...");
  initStorageModules();
  writeVPWord(VP_PROGRESS_ICON, 55);
  delay(100);

  debugLog("runLoadingScreen", "Initializing CH376S USB host...");
  initUSBHost();
  writeVPWord(VP_PROGRESS_ICON, 65);
  delay(100);

  debugLog("runLoadingScreen", "Initializing RC522 RFID...");
  initRFIDModule();
  writeVPWord(VP_PROGRESS_ICON, 75);
  delay(100);

  debugLog("runLoadingScreen", "Initializing fingerprint sensor...");
  initFingerprintModule();
  writeVPWord(VP_PROGRESS_ICON, 85);
  delay(100);

  debugLog("runLoadingScreen", "Initializing QR scanner...");
  initQRScanner();
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
  loadLogsFromSD();
  debugLog("runLoadingScreen", "Loading saved admin PIN...");
  loadAdminPINFromPreferences();

  debugLog("runLoadingScreen", "Loading saved accounts...");
  loadAccountsFromPreferences();
  debugLog("runLoadingScreen", "Loading registered keys...");
  loadKeysFromPreferences();

  writeVPWord(VP_PROGRESS_ICON, 99);
  delay(500);

  debugOK("runLoadingScreen");
  goToLockedScreen();
}
