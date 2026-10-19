// =======================================================
// Pages.ino
// Page routing and page rendering.
// =======================================================

/*
  Function: handlePageCommand()
  Description:
    Handles a general page command from DWIN.

  How to use:
    DGUS Return Key Code:
      VP = 0x1301
      Value = target page ID
*/
bool handlePageCommand(uint16_t vp, uint16_t value) {
  if (vp != VP_PAGE_CMD) return false;

  debugHex16("handlePageCommand", "Page command value", value);

  bool protectedAdminPage =
      value == PAGE_ADMIN_DASHBOARD || value == PAGE_ACCOUNT ||
      value == PAGE_REGISTRATION || value == PAGE_NEW_KEY_REG ||
      value == PAGE_MAINTENANCE_A || value == PAGE_MAINTENANCE_B ||
      value == PAGE_UPDATE_PASSWORD;
  if (protectedAdminPage && !adminAuthenticated) {
    goToAdminLoginPage();
    writeVPWord(VP_PAGE_CMD, 0);
    return true;
  }

  if (value == PAGE_LOCKED_SCREEN) {
    goToLockedScreen();
  }
  else if (value == PAGE_HOME_DASHBOARD_A || value == PAGE_HOME_DASHBOARD_B) {
    goToHomeDashboard(value);
  }
  else if (value == PAGE_QR_BORROWER) {
    goToQRBorrowerPage();
  }
  else if (value == PAGE_ADMIN_LOGIN) {
    goToAdminLoginPage();
  }
  else if (value == PAGE_ADMIN_DASHBOARD) {
    goToAdminDashboard();
  }
  else if (value == PAGE_ACCOUNT) {
    goToAccountPage();
  }
  else if (value == PAGE_UPDATE_PASSWORD) {
    startUpdatePasswordPage();
  }
  else if (value == PAGE_REGISTRATION) {
    goToRegistrationPage();
  }
  else if (value == PAGE_NEW_KEY_REG) {
    startNewKeyRegistrationPage();
  }
  else if (value == PAGE_MAINTENANCE_A || value == PAGE_MAINTENANCE_B) {
    goToMaintenancePage(value);
  }
  else if (value == PAGE_KEYBOARD_LOWER || value == PAGE_KEYBOARD_UPPER || value == PAGE_KEYBOARD_NUMBER) {
    openKeyboardPage(value);
  }
  else {
    debugNumber("handlePageCommand", "Unknown page command", value);
  }

  writeVPWord(VP_PAGE_CMD, 0);
  return true;
}

/*
  Function: isAdminSourcePage()
  Description:
    Checks if a page is allowed to be remembered as an Admin return page.

  How to use:
    Used by rememberAdminReturnPage().
*/
bool isAdminSourcePage(uint16_t pageID) {
  return pageID == PAGE_LOCKED_SCREEN ||
         pageID == PAGE_HOME_DASHBOARD_A ||
         pageID == PAGE_HOME_DASHBOARD_B;
}

/*
  Function: goToLockedScreen()
  Description:
    Opens Page 1 and shows 6 current log cards.

  How to use:
    Called after loading or after Admin Back if Admin came from Page 1.
*/
void goToLockedScreen() {
  debugLog("goToLockedScreen", "Opening Locked Screen Page 1.");
  adminAuthenticated = false;
  clearCurrentHomeUser();
  clearSlotSelections();
  selectedSlotIndex = -1;
  currentPage = PAGE_LOCKED_SCREEN;
  activeCurrentLogCards = CURRENT_LOG_CARDS_6;

  changePage(PAGE_LOCKED_SCREEN);
  delay(120);

  renderCurrentLogs(CURRENT_LOG_CARDS_6);
  updateRTCDisplay();
}

/*
  Function: goToHomeDashboard()
  Description:
    Opens Page 2 or Page 3 and shows 4 current log cards.

  How to use:
    goToHomeDashboard(PAGE_HOME_DASHBOARD_A);
    goToHomeDashboard(PAGE_HOME_DASHBOARD_B);
*/
void goToHomeDashboard(uint16_t pageID) {
  debugNumber("goToHomeDashboard", "Opening Home Dashboard page", pageID);
  adminAuthenticated = false;
  clearSlotSelections();
  selectedSlotIndex = -1;
  currentPage = pageID;
  activeCurrentLogCards = CURRENT_LOG_CARDS_4;

  changePage(pageID);
  delay(120);

  renderCurrentLogs(CURRENT_LOG_CARDS_4);
  renderHomeSlotKeyNames();
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    updateSlotDisplay(i);
  }
  updateRTCDisplay();
}

/*
  Function: goToQRBorrowerPage()
  Description:
    Opens Page 4 after the user selects a slot and presses OK.
    Page 4 supports two borrower-identification methods:
      1. Manual borrower name using the custom keyboard.
      2. QR scan student code, for future student-code database lookup.

  How to use:
    Called when VP_SLOT_CONFIRM is pressed from Page 2 or Page 3.
*/
void goToQRBorrowerPage() {
  debugLog("goToQRBorrowerPage", "Opening QR/Borrower Page 4 with automatic QR sensing.");

  if (selectedSlotIndex < 0) {
    debugFail("goToQRBorrowerPage", "No selected slot. Page 4 will still open but selected slot text will show No Slot.");
  }

  if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
    qrReturnPage = currentPage;
  }

  currentPage = PAGE_QR_BORROWER;

  qrScanActive = false;
  qrBuffer = "";
  lastQRByteTime = 0;
  pendingBorrowerName = "";
  pendingStudentCode = "";
  pendingQRRawData = "";

  changePage(PAGE_QR_BORROWER);
  delay(120);

  renderQRBorrowerPage();
  updateRTCDisplay();

  // Page 4 now behaves like a real QR terminal:
  // it immediately starts listening for QR data without pressing Scan.
  startQRScanForBorrower();
}

/*
  Function: renderQRBorrowerPage()
  Description:
    Redraws the Page 4 borrower-identification data:
      - selected slot
      - manual borrower name
      - scanned student code
      - instruction/status text

  How to use:
    Called when Page 4 opens and after keyboard/QR updates.
*/
void renderQRBorrowerPage() {
  if (selectedSlotIndex >= 0) {
    String slotText = "Slot " + String(selectedSlotIndex + 1);
    writeVPText(VP_QR_SELECTED_SLOT_TEXT, slotText.c_str(), 16);
  } else {
    writeVPText(VP_QR_SELECTED_SLOT_TEXT, "No Slot", 16);
  }

  writeVPText(VP_QR_NAME_TEXT, pendingBorrowerName.c_str(), KEYBOARD_FIELD_BYTES);
  writeVPText(VP_QR_STUDENT_CODE_TEXT, pendingStudentCode.c_str(), 24);

  if (pendingBorrowerName.length() > 0) {
    writeVPText(VP_QR_STATUS_TEXT, "Press OK to confirm", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_SCANNED);
  }
  else if (pendingStudentCode.length() > 0) {
    writeVPText(VP_QR_STATUS_TEXT, "Manual name required", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_MANUAL_REQUIRED);
  }
  else if (qrScanActive) {
    writeVPText(VP_QR_STATUS_TEXT, "Waiting for QR", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_WAITING);
  }
  else {
    writeVPText(VP_QR_STATUS_TEXT, "Preparing QR", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_IDLE);
  }
}

/*
  Function: goToAdminLoginPage()
  Description:
    Opens Page 5 Admin Login first instead of going directly to the
    Admin Dashboard.

    This function saves the exact page where the Admin button was pressed.
    After successful password login, Page 7 Admin Dashboard opens.

  How to use:
    Call this when the Admin button is pressed from:
      Page 1 Locked Screen
      Page 2 Home Dashboard
      Page 3 Home Dashboard

    DGUS recommended Admin button setup:
      Option A:
        VP = 0x1301
        Value = 0x0005
      Option B:
        VP = 0x1042
        Value = 0x0001
*/
void goToAdminLoginPage() {
  debugLog("goToAdminLoginPage", "Opening Admin Login Page 5.");
  rememberAdminReturnPage();

  currentPage = PAGE_ADMIN_LOGIN;

  adminInputValue = 0;
  adminInputText = "";
  adminInputReceived = false;

  changePage(PAGE_ADMIN_LOGIN);
  delay(120);

  renderAdminLoginPage();
  updateRTCDisplay();
}

/*
  Function: renderAdminLoginPage()
  Description:
    Clears the PIN input and displays the Admin Login instructions/status.

  How to use:
    Called automatically when Page 5 Admin Login opens.
*/
void renderAdminLoginPage() {
  writeVPWord(VP_ADMIN_PIN_INPUT, 0);
  writeVPText(VP_ADMIN_LOGIN_STATUS_TEXT, "Enter admin PIN", 32);
  writeVPWord(VP_ADMIN_LOGIN_STATUS_ICON, ADMIN_LOGIN_ICON_WAITING);
}

/*
  Function: goToAdminDashboard()
  Description:
    Opens Page 7 Admin Dashboard after a successful Admin Login.
    It does not force a new return page when coming from Page 5, because
    the return page was already saved before the Admin Login page opened.

  How to use:
    Called after the admin PIN/password is correct.
*/
void goToAdminDashboard() {
  rememberAdminReturnPage();

  currentPage = PAGE_ADMIN_DASHBOARD;

  changePage(PAGE_ADMIN_DASHBOARD);
  delay(120);

  renderTransactionLogs();
  renderDateCards();
  updateRTCDisplay();
}

/*
  Function: goToAccountPage()
  Description:
    Opens Page 8 and renders account cards.

  How to use:
    Called from Admin Dashboard Account button.
*/
void goToAccountPage() {
  debugLog("goToAccountPage", "Opening Account Page 8.");
  currentPage = PAGE_ACCOUNT;

  changePage(PAGE_ACCOUNT);
  delay(120);

  renderAccounts();
  updateRTCDisplay();
}

/*
  Function: goToUpdatePasswordPage()
  Description:
    Opens Page 27 Update Password and redraws the current old/new/confirm
    PIN fields. This function does not clear the pending PIN fields, so it is
    safe to call after returning from the keyboard.

  How to use:
    Called by startUpdatePasswordPage() and keyboardEnter() after PIN typing.
*/
void goToUpdatePasswordPage() {
  debugLog("goToUpdatePasswordPage", "Opening Update Password Page 27.");
  currentPage = PAGE_UPDATE_PASSWORD;

  changePage(PAGE_UPDATE_PASSWORD);
  delay(120);

  renderUpdatePasswordPage();
  updateRTCDisplay();
}

/*
  Function: goToRegistrationPage()
  Description:
    Opens Page 9 and renders registration fields.

  How to use:
    Called from Account Page Register button.
*/
void goToRegistrationPage() {
  debugLog("goToRegistrationPage", "Opening Registration Page 9.");
  currentPage = PAGE_REGISTRATION;

  changePage(PAGE_REGISTRATION);
  delay(120);

  renderRegistrationPage();
  updateRTCDisplay();
}

/*
  Function: startNewKeyRegistrationPage()
  Description:
    Starts a fresh Page 10 key-registration form by clearing the temporary
    key name, slot/key number, and RFID UID fields once.

  How to use:
    Call this only when the user first presses Add New Key from Page 9.
*/
void startNewKeyRegistrationPage() {
  pendingKeyName = "";
  pendingKeyNumber = "";
  pendingKeyRFID = "";

  goToNewKeyRegistrationPage();
}

/*
  Function: goToNewKeyRegistrationPage()
  Description:
    Opens Page 10 and renders the current key-registration fields.
    It does not clear pendingKeyName or pendingKeyNumber so keyboard
    returns can show the typed text correctly.

  How to use:
    Called after returning from the keyboard and by startNewKeyRegistrationPage().
*/
void goToNewKeyRegistrationPage() {
  debugLog("goToNewKeyRegistrationPage", "Opening New Key Registration Page 10.");

  currentPage = PAGE_NEW_KEY_REG;

  changePage(PAGE_NEW_KEY_REG);
  delay(120);

  renderNewKeyRegistrationPage();
  updateRTCDisplay();
}

/*
  Function: goToMaintenancePage()
  Description:
    Opens Page 11 or 12 and renders all 40 slot states.

  How to use:
    Called from Admin Dashboard Maintenance button.
*/
void goToMaintenancePage(uint16_t pageID) {
  debugNumber("goToMaintenancePage", "Opening Maintenance page", pageID);
  currentPage = pageID;

  changePage(pageID);
  delay(120);

  renderMaintenanceSlots();
  updateRTCDisplay();
}

/*
  Function: renderCurrentPage()
  Description:
    Redraws the dynamic objects of the current DWIN page.

  How to use:
    Call after data changes or after returning from keyboard page.
*/
void renderCurrentPage() {
  if (currentPage == PAGE_LOCKED_SCREEN) {
    renderCurrentLogs(CURRENT_LOG_CARDS_6);
  }
  else if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
    renderCurrentLogs(CURRENT_LOG_CARDS_4);
    renderHomeSlotKeyNames();
  }
  else if (currentPage == PAGE_QR_BORROWER) {
    renderQRBorrowerPage();
  }
  else if (currentPage == PAGE_ADMIN_LOGIN) {
    renderAdminLoginPage();
  }
  else if (currentPage == PAGE_ADMIN_DASHBOARD) {
    renderTransactionLogs();
    renderDateCards();
  }
  else if (currentPage == PAGE_ACCOUNT) {
    renderAccounts();
  }
  else if (currentPage == PAGE_UPDATE_PASSWORD) {
    renderUpdatePasswordPage();
  }
  else if (currentPage == PAGE_REGISTRATION) {
    renderRegistrationPage();
  }
  else if (currentPage == PAGE_NEW_KEY_REG) {
    renderNewKeyRegistrationPage();
  }
  else if (currentPage == PAGE_MAINTENANCE_A || currentPage == PAGE_MAINTENANCE_B) {
    renderMaintenanceSlots();
  }

  updateRTCDisplay();
}
