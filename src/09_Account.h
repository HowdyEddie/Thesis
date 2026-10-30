// =======================================================
// Account.ino
// Page 8 account card list, persistence, scrolling, delete, and navigation.
// =======================================================

/*
  Function: loadAccountsFromPreferences()
  Description:
    Loads registered user accounts from ESP32 Preferences memory.

  How to use:
    Called during loading screen.
*/
void loadAccountsFromPreferences() {
  debugLog("loadAccountsFromPreferences", "Loading accounts from ESP32 Preferences.");
  accountCount = 0;
  accountOffset = 0;

  for (uint8_t i = 0; i < MAX_ACCOUNTS; i++) {
    accounts[i].userNumber = "";
    accounts[i].userName = "";
    accounts[i].fingerprintID = 0;
    accounts[i].dateRegistered = "";
    accounts[i].used = false;
  }

  prefs.begin("accounts", true);

  uint8_t savedCount = prefs.getUChar("count", 0);
  if (savedCount > MAX_ACCOUNTS) savedCount = MAX_ACCOUNTS;

  for (uint8_t i = 0; i < savedCount; i++) {
    String keyName = "n" + String(i);
    String keyFinger = "f" + String(i);
    String keyDate = "d" + String(i);

    String name = prefs.getString(keyName.c_str(), "");
    uint16_t fid = prefs.getUShort(keyFinger.c_str(), 0);
    String date = prefs.getString(keyDate.c_str(), "");

    if (name.length() > 0 && fid > 0) {
      accounts[accountCount].userNumber = "U" + String(accountCount + 1);
      accounts[accountCount].userName = name;
      accounts[accountCount].fingerprintID = fid;
      accounts[accountCount].dateRegistered = date;
      accounts[accountCount].used = true;
      accountCount++;
    }
  }

  prefs.end();

  debugNumber("loadAccountsFromPreferences", "Accounts loaded", accountCount);
}

/*
  Function: saveAccountsToPreferences()
  Description:
    Saves all active user accounts to ESP32 Preferences memory.

  How to use:
    Called after registering or deleting an account.
*/
void saveAccountsToPreferences() {
  debugLog("saveAccountsToPreferences", "Saving accounts to ESP32 Preferences.");
  compactAccountsInRAM();

  prefs.begin("accounts", false);
  prefs.clear();

  prefs.putUChar("count", accountCount);

  for (uint8_t i = 0; i < accountCount; i++) {
    String keyName = "n" + String(i);
    String keyFinger = "f" + String(i);
    String keyDate = "d" + String(i);

    prefs.putString(keyName.c_str(), accounts[i].userName);
    prefs.putUShort(keyFinger.c_str(), accounts[i].fingerprintID);
    prefs.putString(keyDate.c_str(), accounts[i].dateRegistered);
  }

  prefs.end();

  debugOK("saveAccountsToPreferences");
}

/*
  Function: compactAccountsInRAM()
  Description:
    Removes deleted/unused account gaps in RAM and rebuilds user numbers.

  How to use:
    Called before saving accounts.
*/
void compactAccountsInRAM() {
  uint8_t writeIndex = 0;

  for (uint8_t readIndex = 0; readIndex < accountCount; readIndex++) {
    if (accounts[readIndex].used) {
      if (writeIndex != readIndex) {
        accounts[writeIndex] = accounts[readIndex];
      }

      accounts[writeIndex].userNumber = "U" + String(writeIndex + 1);
      accounts[writeIndex].used = true;
      writeIndex++;
    }
  }

  for (uint8_t i = writeIndex; i < MAX_ACCOUNTS; i++) {
    accounts[i].userNumber = "";
    accounts[i].userName = "";
    accounts[i].fingerprintID = 0;
    accounts[i].dateRegistered = "";
    accounts[i].used = false;
  }

  accountCount = writeIndex;
}

/*
  Function: addRegisteredAccount()
  Description:
    Adds a registered fingerprint user to RAM, saves it to Preferences,
    and appends a record to the daily SD card.

  How to use:
    Called after successful fingerprint enrollment.
*/
bool addRegisteredAccount(String userName, uint16_t fingerprintID, String dateRegistered) {
  if (userName.length() == 0 || fingerprintID == 0) {
    debugFail("addRegisteredAccount", "Missing name or fingerprint ID.");
    return false;
  }

  if (accountCount >= MAX_ACCOUNTS) {
    debugFail("addRegisteredAccount", "Account list full.");
    return false;
  }

  if (isFingerprintIDRegistered(fingerprintID)) {
    debugFail("addRegisteredAccount", "Fingerprint ID already linked to an account.");
    return false;
  }

  accounts[accountCount].userNumber = "U" + String(accountCount + 1);
  accounts[accountCount].userName = userName;
  accounts[accountCount].fingerprintID = fingerprintID;
  accounts[accountCount].dateRegistered = dateRegistered;
  accounts[accountCount].used = true;
  accountCount++;

  saveAccountsToPreferences();

  String csvLine = getRTCDateTimeString() + "," + accounts[accountCount - 1].userNumber + "," +
                   csvEscapeField(userName) + "," + String(fingerprintID) + "," + dateRegistered;
  appendLineToDaySD("/accounts.csv", csvLine);

  debugLog("addRegisteredAccount", "Registered account saved with fingerprint template.");

  return true;
}

/*
  Function: isFingerprintIDRegistered()
  Description:
    Checks if a fingerprint ID is linked to a saved user account.

  How to use:
    Used before allowing a matched fingerprint to unlock the system.
*/
bool isFingerprintIDRegistered(uint16_t fingerprintID) {
  for (uint8_t i = 0; i < accountCount; i++) {
    if (accounts[i].used && accounts[i].fingerprintID == fingerprintID) {
      return true;
    }
  }

  return false;
}

/*
  Function: getAccountNameByFingerprintID()
  Description:
    Returns the registered account name linked to a fingerprint ID.

  How to use:
    String name = getAccountNameByFingerprintID(3);
*/
String getAccountNameByFingerprintID(uint16_t fingerprintID) {
  for (uint8_t i = 0; i < accountCount; i++) {
    if (accounts[i].used && accounts[i].fingerprintID == fingerprintID) {
      return accounts[i].userName;
    }
  }

  return "";
}

/*
  Function: setCurrentHomeUserFromFingerprint()
  Description:
    Saves the fingerprint user name that opened the Home Dashboard.
    This name is used in the Transaction Log User column.

  How to use:
    Called after a valid Locked Screen fingerprint match.
*/
void setCurrentHomeUserFromFingerprint(uint16_t fingerprintID) {
  currentHomeFingerprintID = fingerprintID;
  currentHomeUserName = getAccountNameByFingerprintID(fingerprintID);

  if (currentHomeUserName.length() == 0) {
    currentHomeUserName = "Unknown";
  }

  debugLog("setCurrentHomeUserFromFingerprint", "Authenticated fingerprint user.");
}

/*
  Function: clearCurrentHomeUser()
  Description:
    Clears the saved Home Dashboard user.

  How to use:
    Called when returning to the Locked Screen.
*/
void clearCurrentHomeUser() {
  currentHomeUserName = "";
  currentHomeFingerprintID = 0;
}


/*
  Function: renderAccounts()
  Description:
    Displays 6 visible user account cards.

  How to use:
    Called when Page 8 opens or when scrolling.
*/
void renderAccounts() {
  for (uint8_t card = 0; card < ACCOUNT_VISIBLE_CARDS; card++) {
    uint8_t index = accountOffset + card;

    if (index >= accountCount || !accounts[index].used) {
      clearAccountCard(card);
      continue;
    }

    uint16_t base = VP_ACCOUNT_CARD_BASE + (card * ACCOUNT_CARD_STEP);
    String registeredDate = accounts[index].dateRegistered;
    if (registeredDate.length() == 10 &&
        registeredDate.charAt(4) == '-' &&
        registeredDate.charAt(7) == '-') {
      registeredDate = registeredDate.substring(5, 7) + "/" +
                       registeredDate.substring(8, 10) + "/" +
                       registeredDate.substring(2, 4);
    }

    writeVPText(base + ACC_USERNO_OFFSET, accounts[index].userNumber.c_str(), ACC_USERNO_BYTES);
    writeVPText(base + ACC_USERNAME_OFFSET, accounts[index].userName.c_str(), ACC_USERNAME_BYTES);
    writeVPText(base + ACC_FINGERID_OFFSET, String(accounts[index].fingerprintID).c_str(), ACC_FINGERID_BYTES);
    writeVPText(base + ACC_DATE_OFFSET, registeredDate.c_str(), ACC_DATE_BYTES);
  }
}

/*
  Function: clearAccountCard()
  Description:
    Clears one visible account card.

  How to use:
    clearAccountCard(0);
*/
void clearAccountCard(uint8_t cardIndex) {
  uint16_t base = VP_ACCOUNT_CARD_BASE + (cardIndex * ACCOUNT_CARD_STEP);

  writeVPText(base + ACC_USERNO_OFFSET, "", ACC_USERNO_BYTES);
  writeVPText(base + ACC_USERNAME_OFFSET, "", ACC_USERNAME_BYTES);
  writeVPText(base + ACC_FINGERID_OFFSET, "", ACC_FINGERID_BYTES);
  writeVPText(base + ACC_DATE_OFFSET, "", ACC_DATE_BYTES);
}

/*
  Function: handleAccountControls()
  Description:
    Handles all Page 8 account controls.

  How to use:
    Called automatically from handleDWIN().
*/
bool handleAccountControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_ACCOUNT) return false;

  if (vp == VP_ACCOUNT_SCROLL_UP) {
    scrollAccountsUp();
    writeVPWord(VP_ACCOUNT_SCROLL_UP, 0);
    return true;
  }

  if (vp == VP_ACCOUNT_SCROLL_DOWN) {
    scrollAccountsDown();
    writeVPWord(VP_ACCOUNT_SCROLL_DOWN, 0);
    return true;
  }

  if (vp == VP_ACCOUNT_REGISTER) {
    writeVPWord(VP_ACCOUNT_REGISTER, 0);
    pendingRegisterUserName = "";
    goToRegistrationPage();
    return true;
  }

  if (vp == VP_ACCOUNT_UPDATE_PASSWORD) {
    writeVPWord(VP_ACCOUNT_UPDATE_PASSWORD, 0);
    startUpdatePasswordPage();
    return true;
  }

  if (vp == VP_ACCOUNT_BACK) {
    writeVPWord(VP_ACCOUNT_BACK, 0);
    goToAdminDashboard();
    return true;
  }

  if (vp == VP_ACCOUNT_DELETE) {
    if (value >= 1 && value <= ACCOUNT_VISIBLE_CARDS) {
      deleteVisibleAccount(value - 1);
    }

    writeVPWord(VP_ACCOUNT_DELETE, 0);
    return true;
  }

  return false;
}

/*
  Function: scrollAccountsUp()
  Description:
    Scrolls account cards upward.

  How to use:
    Called by VP_ACCOUNT_SCROLL_UP.
*/
void scrollAccountsUp() {
  if (accountOffset > 0) {
    accountOffset--;
    renderAccounts();
  }
}

/*
  Function: scrollAccountsDown()
  Description:
    Scrolls account cards downward.

  How to use:
    Called by VP_ACCOUNT_SCROLL_DOWN.
*/
void scrollAccountsDown() {
  if (accountOffset + ACCOUNT_VISIBLE_CARDS < accountCount) {
    accountOffset++;
    renderAccounts();
  }
}

/*
  Function: deleteVisibleAccount()
  Description:
    Deletes an account and also deletes its fingerprint template from the sensor.

  How to use:
    Called when VP_ACCOUNT_DELETE sends value 1 to 6.
*/
void deleteVisibleAccount(uint8_t visibleIndex) {
  uint8_t index = accountOffset + visibleIndex;

  if (index >= accountCount || !accounts[index].used) return;

  uint16_t fid = accounts[index].fingerprintID;

  if (fid == 0) {
    debugFail("deleteVisibleAccount", "Account has no valid fingerprint ID; account was not deleted.");
    return;
  }

  if (!fpsReady && !initFingerprintModule()) {
    debugFail("deleteVisibleAccount", "Fingerprint sensor is unavailable; account was not deleted.");
    return;
  }

  debugLog("deleteVisibleAccount", "Deleting linked fingerprint template.");
  uint8_t result = finger.deleteModel(fid);
  debugNumber("deleteVisibleAccount", "finger.deleteModel result", result);

  if (result != FINGERPRINT_OK) {
    debugFail("deleteVisibleAccount", "Fingerprint template deletion failed; account was not deleted.");
    return;
  }

  if (currentHomeFingerprintID == fid) {
    clearCurrentHomeUser();
  }

  accounts[index].used = false;
  saveAccountsToPreferences();

  if (accountOffset >= accountCount && accountOffset > 0) {
    accountOffset--;
  }

  renderAccounts();
}
