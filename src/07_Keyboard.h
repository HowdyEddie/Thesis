// =======================================================
// Keyboard.ino
// Custom DWIN keyboard separated from the main project.
// =======================================================

/*
  Function: handleKeyboard()
  Description:
    Handles custom keyboard buttons from DWIN.
    Every keyboard key sends to VP 0x1450.

  How to use:
    DGUS keyboard button:
      VP = 0x1450
      Value = ASCII value or special key value.
*/
bool handleKeyboard(uint16_t vp, uint16_t value) {
  if (vp != VP_KEYBOARD_CMD) return false;

  if (value == KEY_LOWER) {
    openKeyboardPage(PAGE_KEYBOARD_LOWER);
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_UPPER) {
    openKeyboardPage(PAGE_KEYBOARD_UPPER);
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_NUMBER) {
    openKeyboardPage(PAGE_KEYBOARD_NUMBER);
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_SHIFT) {
    toggleKeyboardCase();
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_BACKSPACE) {
    keyboardBackspace();
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_CLEAR) {
    keyboardClear();
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_ENTER) {
    keyboardEnter();
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value == KEY_SPACE) {
    keyboardAddChar(' ');
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  if (value >= 0x0020 && value <= 0x007E) {
    keyboardAddChar((char)value);
    writeVPWord(VP_KEYBOARD_CMD, 0);
    return true;
  }

  Serial.println("Unknown keyboard key ignored.");
  writeVPWord(VP_KEYBOARD_CMD, 0);
  return true;
}

/*
  Function: openKeyboard()
  Description:
    Opens the custom keyboard and remembers what field will receive the text.

  How to use:
    openKeyboard(KB_TARGET_REGISTER_USER_NAME, PAGE_REGISTRATION, false);
*/
void openKeyboard(KeyboardTarget target, uint16_t returnPage, bool numberMode) {
  keyboardTarget = target;
  keyboardReturnPage = returnPage;
  keyboardText = "";

  writeVPText(VP_KEYBOARD_TEXT, "", KEYBOARD_FIELD_BYTES);

  if (numberMode) {
    openKeyboardPage(PAGE_KEYBOARD_NUMBER);
  } else {
    openKeyboardPage(PAGE_KEYBOARD_LOWER);
  }
}

/*
  Function: openKeyboardPage()
  Description:
    Opens one keyboard page and redraws the typed text.

  How to use:
    openKeyboardPage(PAGE_KEYBOARD_UPPER);
*/
void openKeyboardPage(uint16_t pageID) {
  currentPage = pageID;

  if (pageID == PAGE_KEYBOARD_UPPER) keyboardUpperMode = true;
  if (pageID == PAGE_KEYBOARD_LOWER) keyboardUpperMode = false;

  changePage(pageID);
  delay(100);

  writeVPText(VP_KEYBOARD_TEXT, keyboardText.c_str(), KEYBOARD_FIELD_BYTES);
  updateRTCDisplay();
}

/*
  Function: toggleKeyboardCase()
  Description:
    Switches between lowercase and uppercase keyboard pages.

  How to use:
    Called when Shift/Caps sends 0xF001.
*/
void toggleKeyboardCase() {
  if (keyboardUpperMode) {
    openKeyboardPage(PAGE_KEYBOARD_LOWER);
  } else {
    openKeyboardPage(PAGE_KEYBOARD_UPPER);
  }
}

/*
  Function: keyboardAddChar()
  Description:
    Adds one character to keyboardText and updates the DWIN text display.

  How to use:
    Automatically called when a printable ASCII key is pressed.
*/
void keyboardAddChar(char c) {
  if (keyboardText.length() >= KEYBOARD_FIELD_BYTES) {
    Serial.println("Keyboard field full.");
    return;
  }

  keyboardText += c;
  writeVPText(VP_KEYBOARD_TEXT, keyboardText.c_str(), KEYBOARD_FIELD_BYTES);
}

/*
  Function: keyboardBackspace()
  Description:
    Deletes the last typed character.

  How to use:
    Called when Backspace sends 0x0008.
*/
void keyboardBackspace() {
  if (keyboardText.length() > 0) {
    keyboardText.remove(keyboardText.length() - 1);
  }

  writeVPText(VP_KEYBOARD_TEXT, keyboardText.c_str(), KEYBOARD_FIELD_BYTES);
}

/*
  Function: keyboardClear()
  Description:
    Clears the whole typed text.

  How to use:
    Called when Clear sends 0xF002.
*/
void keyboardClear() {
  keyboardText = "";
  writeVPText(VP_KEYBOARD_TEXT, keyboardText.c_str(), KEYBOARD_FIELD_BYTES);
}

/*
  Function: keyboardEnter()
  Description:
    Applies the typed text to the active target field, then returns
    to the page that opened the keyboard.

  How to use:
    Called when Enter/OK sends 0x000D.
*/
void keyboardEnter() {
  debugNumber("keyboardEnter", "Input length", keyboardText.length());

  if (keyboardTarget == KB_TARGET_REGISTER_USER_NAME) {
    pendingRegisterUserName = keyboardText;
  }
  else if (keyboardTarget == KB_TARGET_KEY_NAME) {
    pendingKeyName = keyboardText;
  }
  else if (keyboardTarget == KB_TARGET_KEY_NUMBER) {
    pendingKeyNumber = keyboardText;
  }
  else if (keyboardTarget == KB_TARGET_ADMIN_PIN) {
    adminInputText = normalizePINText(keyboardText);
    adminInputValue = (uint16_t)adminInputText.toInt();
    adminInputReceived = true;
  }
  else if (keyboardTarget == KB_TARGET_BORROWER_NAME) {
    pendingBorrowerName = keyboardText;
  }
  else if (keyboardTarget == KB_TARGET_UPDATE_OLD_PIN) {
    pendingOldAdminPIN = normalizePINText(keyboardText);
  }
  else if (keyboardTarget == KB_TARGET_UPDATE_NEW_PIN) {
    pendingNewAdminPIN = normalizePINText(keyboardText);
  }
  else if (keyboardTarget == KB_TARGET_UPDATE_CONFIRM_PIN) {
    pendingConfirmAdminPIN = normalizePINText(keyboardText);
  }

  keyboardTarget = KB_TARGET_NONE;

  if (keyboardReturnPage == PAGE_REGISTRATION) {
    goToRegistrationPage();
  }
  else if (keyboardReturnPage == PAGE_NEW_KEY_REG) {
    goToNewKeyRegistrationPage();
  }
  else if (keyboardReturnPage == PAGE_QR_BORROWER) {
    currentPage = PAGE_QR_BORROWER;
    changePage(PAGE_QR_BORROWER);
    delay(100);
    renderQRBorrowerPage();
  }
  else if (keyboardReturnPage == PAGE_UPDATE_PASSWORD) {
    goToUpdatePasswordPage();
  }
  else if (keyboardReturnPage == PAGE_ADMIN_DASHBOARD) {
    goToAdminDashboard();
  }
  else {
    currentPage = keyboardReturnPage;
    changePage(keyboardReturnPage);
    delay(100);
    renderCurrentPage();
  }
}

/*
  Function: getKeyboardText()
  Description:
    Returns the current typed keyboard text.

  How to use:
    String text = getKeyboardText();
*/
String getKeyboardText() {
  return keyboardText;
}
