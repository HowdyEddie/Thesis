// =======================================================
// Registration.ino
// Page 9 user registration and fingerprint enrollment.
// =======================================================

/*
  Function: handleRegistrationControls()
  Description:
    Handles Page 9 registration controls.

  How to use:
    Called automatically from handleDWIN().
*/
bool handleRegistrationControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_REGISTRATION) return false;

  if (vp == VP_REGISTER_NAME_BUTTON) {
    writeVPWord(VP_REGISTER_NAME_BUTTON, 0);
    openKeyboard(KB_TARGET_REGISTER_USER_NAME, PAGE_REGISTRATION, false);
    return true;
  }

  if (vp == VP_REGISTER_OK) {
    writeVPWord(VP_REGISTER_OK, 0);
    startUserFingerprintRegistration();
    return true;
  }

  if (vp == VP_REGISTER_ADD_NEW_KEY) {
    writeVPWord(VP_REGISTER_ADD_NEW_KEY, 0);
    startNewKeyRegistrationPage();
    return true;
  }

  if (vp == VP_REGISTER_BACK) {
    writeVPWord(VP_REGISTER_BACK, 0);
    goToAccountPage();
    return true;
  }

  return false;
}

/*
  Function: renderRegistrationPage()
  Description:
    Displays the current user name and registration status.

  How to use:
    Called when Page 9 opens and after keyboard input.
*/
void renderRegistrationPage() {
  writeVPText(VP_REGISTER_NAME_TEXT, pendingRegisterUserName.c_str(), KEYBOARD_FIELD_BYTES);
  writeVPText(VP_REGISTER_STATUS_TEXT, "Ready", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_IDLE);
}

/*
  Function: startUserFingerprintRegistration()
  Description:
    Registers a real fingerprint using the R503 capacitive FPS module,
    verifies that the model was stored, then saves the user account.

  How to use:
    Type user name first, return to Page 9, then press OK VP 0x3841.
*/
void startUserFingerprintRegistration() {
  pendingRegisterUserName.trim();

  if (pendingRegisterUserName.length() == 0) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Enter user name first", 32);
    writeVPWord(VP_REGISTER_STATUS_ICON, REG_FAILED);
    debugFail("startUserFingerprintRegistration", "No user name.");
    return;
  }

  if (!fpsReady) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "FPS not ready", 32);
    writeVPWord(VP_REGISTER_STATUS_ICON, REG_FAILED);
    debugFail("startUserFingerprintRegistration", "FPS not ready.");
    return;
  }

  pendingFingerprintID = findFreeFingerprintID();

  if (pendingFingerprintID == 0) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "No free FPS ID", 32);
    writeVPWord(VP_REGISTER_STATUS_ICON, REG_FAILED);
    debugFail("startUserFingerprintRegistration", "No free fingerprint ID.");
    return;
  }

  debugNumber("startUserFingerprintRegistration", "User name length", pendingRegisterUserName.length());
  debugLog("startUserFingerprintRegistration", "Fingerprint template slot selected.");

  writeVPText(VP_REGISTER_STATUS_TEXT, "Registering FPS", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_REGISTERING);

  bool ok = enrollFingerprintBlocking(pendingFingerprintID);

  finger.getTemplateCount();
  debugNumber("startUserFingerprintRegistration", "Template count after registration", finger.templateCount);

  if (!ok) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Register failed", 32);
    writeVPWord(VP_REGISTER_STATUS_ICON, REG_FAILED);
    debugFail("startUserFingerprintRegistration", "Fingerprint enrollment returned false.");
    return;
  }

  if (!addRegisteredAccount(pendingRegisterUserName, pendingFingerprintID, getRTCDateString())) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Save account failed", 32);
    writeVPWord(VP_REGISTER_STATUS_ICON, REG_FAILED);
    debugFail("startUserFingerprintRegistration", "Fingerprint stored but account save failed.");
    return;
  }

  writeVPText(VP_REGISTER_STATUS_TEXT, "Register success", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_SUCCESS);

  debugLog("startUserFingerprintRegistration", "Registration completed.");
  debugLog("startUserFingerprintRegistration", "Fingerprint template linked to account.");

  pendingRegisterUserName = "";
  pendingFingerprintID = 0;
}
