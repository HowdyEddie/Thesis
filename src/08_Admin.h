// =======================================================
// Admin.ino
// Admin Login Page, Admin Dashboard navigation, and Admin Back.
// =======================================================

/*
  Function: handleAdminAccess()
  Description:
    Handles Admin button, Admin PIN input, Login button, Back button,
    Account button, Maintenance button, and Download button.

    Updated rule:
      Admin button must open Page 5 Admin Login first.
      It must not go directly to Page 7 Admin Dashboard.

  How to use:
    Admin button from Page 1, 2, or 3:
      Preferred: VP 0x1042 value 0x0001
      Alternative: VP 0x1301 value 0x0005

    Admin Login Page:
      PIN input VP = 0x1040
      Login VP    = 0x1043, value 0x0001
      Back VP     = 0x1044, value 0x0001

    Admin Dashboard:
      Back VP        = 0x1044, value 0x0001
      Account VP     = 0x1045, value 0x0001
      Maintenance VP = 0x1046, value 0x0001
      Download VP    = 0x1047, value 0x0001
*/
bool handleAdminAccess(uint16_t vp, uint16_t value) {
  // Admin button from Locked/Home pages.
  // This opens Admin Login first, not the Admin Dashboard.
  if (vp == VP_ADMIN_OPEN) {
    debugLog("handleAdminAccess", "Admin button pressed. Opening Admin Login page.");
    writeVPWord(VP_ADMIN_OPEN, 0);
    goToAdminLoginPage();
    return true;
  }

  // Admin PIN input. This only stores the input.
  // The password is checked only after the Login button is pressed.
  if (vp == VP_ADMIN_PIN_INPUT) {
    adminInputValue = value;
    adminInputText = pinValueToFourDigitText(value);
    adminInputReceived = true;

    debugHex16("handleAdminAccess", "Admin PIN input raw value", adminInputValue);
    debugValue("handleAdminAccess", "Admin PIN input text", adminInputText);

    return true;
  }

  // Login button. This should be used only on Page 5 Admin Login.
  if (vp == VP_ADMIN_LOGIN) {
    debugLog("handleAdminAccess", "Admin Login button pressed.");
    writeVPWord(VP_ADMIN_LOGIN, 0);

    if (currentPage != PAGE_ADMIN_LOGIN) {
      debugFail("handleAdminAccess", "Login ignored because Admin Login page is not open.");
      return true;
    }

    if (!adminInputReceived) {
      debugFail("handleAdminAccess", "No admin PIN received yet.");
      writeVPText(VP_ADMIN_LOGIN_STATUS_TEXT, "Enter PIN first", 32);
      writeVPWord(VP_ADMIN_LOGIN_STATUS_ICON, ADMIN_LOGIN_ICON_DENIED);
      return true;
    }

    if (adminPINMatches(adminInputText)) {
      debugOK("handleAdminAccess");
      adminAuthenticated = true;

      adminInputValue = 0;
      adminInputText = "";
      adminInputReceived = false;

      writeVPWord(VP_ADMIN_PIN_INPUT, 0);
      writeVPText(VP_ADMIN_LOGIN_STATUS_TEXT, "Access granted", 32);
      writeVPWord(VP_ADMIN_LOGIN_STATUS_ICON, ADMIN_LOGIN_ICON_GRANTED);
      delay(300);

      goToAdminDashboard();
    } else {
      Serial.println("Wrong admin PIN.");

      adminInputValue = 0;
      adminInputText = "";
      adminInputReceived = false;

      writeVPWord(VP_ADMIN_PIN_INPUT, 0);
      writeVPText(VP_ADMIN_LOGIN_STATUS_TEXT, "Wrong PIN", 32);
      writeVPWord(VP_ADMIN_LOGIN_STATUS_ICON, ADMIN_LOGIN_ICON_DENIED);
    }

    return true;
  }

  // Back button is shared by Admin Login Page and Admin Dashboard.
  if (vp == VP_ADMIN_BACK) {
    if (currentPage != PAGE_ADMIN_LOGIN &&
        (currentPage != PAGE_ADMIN_DASHBOARD || !adminAuthenticated)) {
      writeVPWord(VP_ADMIN_BACK, 0);
      return true;
    }

    Serial.println("Admin Back pressed.");
    writeVPWord(VP_ADMIN_BACK, 0);

    if (currentPage == PAGE_ADMIN_LOGIN) {
      goBackFromAdminLogin();
    } else {
      goBackFromAdminDashboard();
    }

    return true;
  }

  if (vp == VP_ADMIN_ACCOUNT) {
    if (currentPage != PAGE_ADMIN_DASHBOARD || !adminAuthenticated) {
      writeVPWord(VP_ADMIN_ACCOUNT, 0);
      if (!adminAuthenticated) goToAdminLoginPage();
      return true;
    }

    Serial.println("Admin Account pressed.");
    writeVPWord(VP_ADMIN_ACCOUNT, 0);
    goToAccountPage();
    return true;
  }

  if (vp == VP_ADMIN_MAINTENANCE) {
    if (currentPage != PAGE_ADMIN_DASHBOARD || !adminAuthenticated) {
      writeVPWord(VP_ADMIN_MAINTENANCE, 0);
      if (!adminAuthenticated) goToAdminLoginPage();
      return true;
    }

    Serial.println("Admin Maintenance pressed.");
    writeVPWord(VP_ADMIN_MAINTENANCE, 0);
    goToMaintenancePage(PAGE_MAINTENANCE_A);
    return true;
  }

  if (vp == VP_ADMIN_DOWNLOAD) {
    if (currentPage != PAGE_ADMIN_DASHBOARD || !adminAuthenticated) {
      writeVPWord(VP_ADMIN_DOWNLOAD, 0);
      if (!adminAuthenticated) goToAdminLoginPage();
      return true;
    }

    Serial.println("Admin Download pressed.");
    writeVPWord(VP_ADMIN_DOWNLOAD, 0);
    handleAdminDownload();
    return true;
  }

  return false;
}

/*
  Function: rememberAdminReturnPage()
  Description:
    Saves the exact page where Admin was pressed from.

    Valid source pages:
      Page 1 Locked Screen
      Page 2 Home Dashboard
      Page 3 Home Dashboard

  How to use:
    Called automatically by goToAdminLoginPage().
*/
void rememberAdminReturnPage() {
  if (isAdminSourcePage(currentPage)) {
    adminReturnPage = currentPage;

    Serial.print("Admin return page saved: ");
    Serial.println(adminReturnPage);
  }
}

/*
  Function: goBackFromAdminLogin()
  Description:
    Returns from Page 5 Admin Login to the exact page where Admin was pressed.

  How to use:
    Called when Back VP 0x1044 is pressed while currentPage == PAGE_ADMIN_LOGIN.
*/
void goBackFromAdminLogin() {
  Serial.print("Returning from Admin Login to saved page: ");
  Serial.println(adminReturnPage);

  adminInputValue = 0;
  adminInputText = "";
  adminInputReceived = false;
  writeVPWord(VP_ADMIN_PIN_INPUT, 0);

  if (adminReturnPage == PAGE_LOCKED_SCREEN) {
    goToLockedScreen();
  }
  else if (adminReturnPage == PAGE_HOME_DASHBOARD_A || adminReturnPage == PAGE_HOME_DASHBOARD_B) {
    goToHomeDashboard(adminReturnPage);
  }
  else {
    goToLockedScreen();
  }
}

/*
  Function: goBackFromAdminDashboard()
  Description:
    Returns from Page 7 Admin Dashboard to the exact page where Admin was pressed.

  How to use:
    Called when Back VP 0x1044 is pressed while currentPage == PAGE_ADMIN_DASHBOARD.
*/
void goBackFromAdminDashboard() {
  Serial.print("Returning from Admin Dashboard to saved page: ");
  Serial.println(adminReturnPage);
  adminAuthenticated = false;

  if (adminReturnPage == PAGE_LOCKED_SCREEN) {
    goToLockedScreen();
  }
  else if (adminReturnPage == PAGE_HOME_DASHBOARD_A || adminReturnPage == PAGE_HOME_DASHBOARD_B) {
    goToHomeDashboard(adminReturnPage);
  }
  else {
    goToLockedScreen();
  }
}

/*
  Function: loadAdminPINFromPreferences()
  Description:
    Loads the saved 4-digit Admin PIN from ESP32 Preferences.
    If no saved PIN exists, it uses ADMIN_DEFAULT_PIN_TEXT.

  How to use:
    Called once during loading screen before Admin Login is used.
*/
void loadAdminPINFromPreferences() {
  prefs.begin("admin", true);
  savedAdminPIN = prefs.getString("pin", ADMIN_DEFAULT_PIN_TEXT);
  prefs.end();

  savedAdminPIN = normalizePINText(savedAdminPIN);

  if (!isValidFourDigitPIN(savedAdminPIN)) {
    savedAdminPIN = ADMIN_DEFAULT_PIN_TEXT;
  }

  debugValue("loadAdminPINFromPreferences", "Saved admin PIN loaded", "****");
}

/*
  Function: saveAdminPINToPreferences()
  Description:
    Saves a validated 4-digit Admin PIN to ESP32 Preferences.

  How to use:
    saveAdminPINToPreferences("1234");
*/
void saveAdminPINToPreferences(String pinText) {
  pinText = normalizePINText(pinText);

  if (!isValidFourDigitPIN(pinText)) {
    debugFail("saveAdminPINToPreferences", "PIN must be exactly 4 numeric digits.");
    return;
  }

  prefs.begin("admin", false);
  prefs.putString("pin", pinText);
  prefs.end();

  savedAdminPIN = pinText;
  debugOK("saveAdminPINToPreferences");
}

/*
  Function: normalizePINText()
  Description:
    Keeps only numeric digits from a PIN string and limits it to 4 digits.

  How to use:
    String cleanPin = normalizePINText(keyboardText);
*/
String normalizePINText(String pinText) {
  String cleaned = "";
  pinText.trim();

  for (uint16_t i = 0; i < pinText.length(); i++) {
    char c = pinText.charAt(i);
    if (isDigit(c) && cleaned.length() < ADMIN_PIN_DIGITS) {
      cleaned += c;
    }
  }

  return cleaned;
}

/*
  Function: pinValueToFourDigitText()
  Description:
    Converts a DWIN numeric input word into a 4-digit PIN string.
    Values from 0 to 9999 are zero-padded, so 7 becomes "0007".

  How to use:
    adminInputText = pinValueToFourDigitText(valueFromDWIN);
*/
String pinValueToFourDigitText(uint16_t value) {
  if (value == ADMIN_PASS_VALUE) {
    // Compatibility with old projects that returned 0x3030 for the default PIN.
    return ADMIN_DEFAULT_PIN_TEXT;
  }

  if (value <= 9999) {
    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%04u", value);
    return String(buffer);
  }

  return String(value);
}

/*
  Function: isValidFourDigitPIN()
  Description:
    Checks if a PIN is exactly 4 numeric digits.

  How to use:
    if (isValidFourDigitPIN(newPin)) { ... }
*/
bool isValidFourDigitPIN(String pinText) {
  if (pinText.length() != ADMIN_PIN_DIGITS) return false;

  for (uint8_t i = 0; i < ADMIN_PIN_DIGITS; i++) {
    if (!isDigit(pinText.charAt(i))) return false;
  }

  return true;
}

/*
  Function: adminPINMatches()
  Description:
    Compares typed Admin PIN text with the saved Admin PIN.

  How to use:
    if (adminPINMatches(adminInputText)) goToAdminDashboard();
*/
bool adminPINMatches(String pinText) {
  pinText = normalizePINText(pinText);
  return isValidFourDigitPIN(pinText) && pinText == savedAdminPIN;
}

/*
  Function: handleAdminDownload()
  Description:
    Handles the Admin Dashboard Download button.

    Laptop-like USB rule:
      - USB insertion is watched in the background by monitorUSBInsertion().
      - When Download is pressed, the code checks again if USB is still inserted.
      - The actual copy attempt happens only after pressing Download.

  How to use:
    Called when VP_ADMIN_DOWNLOAD is pressed.
*/
void handleAdminDownload() {
  if (selectedDateText.length() == 0) {
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_FAILED);
    debugFail("handleAdminDownload", "No selected date log.");
    return;
  }

  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_CHECKING);
  debugValue("handleAdminDownload", "Selected date", selectedDateText);

  usbInserted = usbHostConnected();
  previousUSBInserted = usbInserted;

  if (!usbInserted) {
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_NOT_FOUND);
    debugFail("handleAdminDownload", "USB not inserted or not detected.");
    return;
  }

  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_CONNECTED);
  delay(300);

  downloadSelectedDateLogToUSB();
}
