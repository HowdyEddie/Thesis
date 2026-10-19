// =======================================================
// UpdatePassword.ino
// Page 27 Admin Password update feature.
// =======================================================

/*
  Function: startUpdatePasswordPage()
  Description:
    Opens Page 27 as a fresh Update Password form by clearing the temporary
    old, new, and confirm PIN fields.

  How to use:
    Called when the Account Page Update Password button is pressed.
*/
void startUpdatePasswordPage() {
  clearUpdatePasswordFields();
  goToUpdatePasswordPage();
}

/*
  Function: clearUpdatePasswordFields()
  Description:
    Clears only the temporary Page 27 PIN inputs.
    It does not change the saved Admin PIN.

  How to use:
    Called before opening Page 27 as a new form and after a successful save.
*/
void clearUpdatePasswordFields() {
  pendingOldAdminPIN = "";
  pendingNewAdminPIN = "";
  pendingConfirmAdminPIN = "";
}

/*
  Function: renderUpdatePasswordPage()
  Description:
    Redraws the old PIN, new PIN, confirm PIN, and status objects on Page 27.
    The PIN fields are shown as **** when a value is typed.

  How to use:
    Called when Page 27 opens and after keyboard input returns.
*/
void renderUpdatePasswordPage() {
  writeVPText(VP_UPDATE_PASS_OLD_TEXT, pendingOldAdminPIN.length() > 0 ? "****" : "", UPDATE_PASS_FIELD_BYTES);
  writeVPText(VP_UPDATE_PASS_NEW_TEXT, pendingNewAdminPIN.length() > 0 ? "****" : "", UPDATE_PASS_FIELD_BYTES);
  writeVPText(VP_UPDATE_PASS_CONFIRM_TEXT, pendingConfirmAdminPIN.length() > 0 ? "****" : "", UPDATE_PASS_FIELD_BYTES);

  if (pendingOldAdminPIN.length() == 0) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Enter old 4-digit PIN", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_IDLE);
  }
  else if (pendingNewAdminPIN.length() == 0) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Enter new 4-digit PIN", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_EDITING);
  }
  else if (pendingConfirmAdminPIN.length() == 0) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Confirm new PIN", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_EDITING);
  }
  else {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Press OK to save", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_EDITING);
  }
}

/*
  Function: handleUpdatePasswordControls()
  Description:
    Handles Page 27 Update Password buttons:
      - Old PIN keyboard
      - New PIN keyboard
      - Confirm PIN keyboard
      - Save/OK
      - Back

  How to use:
    Called automatically by handleDWIN().
*/
bool handleUpdatePasswordControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_UPDATE_PASSWORD) return false;

  if (vp == VP_UPDATE_PASS_OLD_BUTTON) {
    writeVPWord(VP_UPDATE_PASS_OLD_BUTTON, 0);
    openUpdatePasswordKeyboard(KB_TARGET_UPDATE_OLD_PIN);
    return true;
  }

  if (vp == VP_UPDATE_PASS_NEW_BUTTON) {
    writeVPWord(VP_UPDATE_PASS_NEW_BUTTON, 0);
    openUpdatePasswordKeyboard(KB_TARGET_UPDATE_NEW_PIN);
    return true;
  }

  if (vp == VP_UPDATE_PASS_CONFIRM_BUTTON) {
    writeVPWord(VP_UPDATE_PASS_CONFIRM_BUTTON, 0);
    openUpdatePasswordKeyboard(KB_TARGET_UPDATE_CONFIRM_PIN);
    return true;
  }

  if (vp == VP_UPDATE_PASS_SAVE) {
    writeVPWord(VP_UPDATE_PASS_SAVE, 0);
    saveUpdatedAdminPassword();
    return true;
  }

  if (vp == VP_UPDATE_PASS_BACK) {
    writeVPWord(VP_UPDATE_PASS_BACK, 0);
    clearUpdatePasswordFields();
    goToAccountPage();
    return true;
  }

  return false;
}

/*
  Function: openUpdatePasswordKeyboard()
  Description:
    Opens the number keyboard for one Page 27 PIN field.
    The existing value is cleared so the user can retype the 4-digit PIN.

  How to use:
    openUpdatePasswordKeyboard(KB_TARGET_UPDATE_NEW_PIN);
*/
void openUpdatePasswordKeyboard(KeyboardTarget target) {
  if (target == KB_TARGET_UPDATE_OLD_PIN) {
    pendingOldAdminPIN = "";
  }
  else if (target == KB_TARGET_UPDATE_NEW_PIN) {
    pendingNewAdminPIN = "";
  }
  else if (target == KB_TARGET_UPDATE_CONFIRM_PIN) {
    pendingConfirmAdminPIN = "";
  }

  openKeyboard(target, PAGE_UPDATE_PASSWORD, true);
}

/*
  Function: saveUpdatedAdminPassword()
  Description:
    Validates and saves the new 4-digit Admin PIN.
    The old PIN must match the currently saved PIN, and the new PIN and
    confirm PIN must match.

  How to use:
    Called when Page 27 OK/Save sends VP 0x3B33.
*/
void saveUpdatedAdminPassword() {
  pendingOldAdminPIN = normalizePINText(pendingOldAdminPIN);
  pendingNewAdminPIN = normalizePINText(pendingNewAdminPIN);
  pendingConfirmAdminPIN = normalizePINText(pendingConfirmAdminPIN);

  if (!isValidFourDigitPIN(pendingOldAdminPIN)) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Old PIN must be 4 digits", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_ERROR);
    debugFail("saveUpdatedAdminPassword", "Old PIN is invalid.");
    return;
  }

  if (!adminPINMatches(pendingOldAdminPIN)) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Old PIN is wrong", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_ERROR);
    debugFail("saveUpdatedAdminPassword", "Old PIN does not match saved Admin PIN.");
    return;
  }

  if (!isValidFourDigitPIN(pendingNewAdminPIN)) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "New PIN must be 4 digits", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_ERROR);
    debugFail("saveUpdatedAdminPassword", "New PIN is invalid.");
    return;
  }

  if (pendingNewAdminPIN != pendingConfirmAdminPIN) {
    writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "New PINs do not match", UPDATE_PASS_STATUS_BYTES);
    writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_ERROR);
    debugFail("saveUpdatedAdminPassword", "New PIN and confirm PIN do not match.");
    return;
  }

  saveAdminPINToPreferences(pendingNewAdminPIN);
  clearUpdatePasswordFields();

  writeVPText(VP_UPDATE_PASS_OLD_TEXT, "", UPDATE_PASS_FIELD_BYTES);
  writeVPText(VP_UPDATE_PASS_NEW_TEXT, "", UPDATE_PASS_FIELD_BYTES);
  writeVPText(VP_UPDATE_PASS_CONFIRM_TEXT, "", UPDATE_PASS_FIELD_BYTES);
  writeVPText(VP_UPDATE_PASS_STATUS_TEXT, "Password updated", UPDATE_PASS_STATUS_BYTES);
  writeVPWord(VP_UPDATE_PASS_STATUS_ICON, UPDATE_PASS_ICON_SAVED);

  debugOK("saveUpdatedAdminPassword");
}
