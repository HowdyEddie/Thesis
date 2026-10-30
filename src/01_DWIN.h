// =======================================================
// DWIN.ino
// Handles DWIN UART frames, writes VP values/text, and changes pages.
// =======================================================

/*
  Function: handleDWIN()
  Description:
    Receives one DWIN VP/value event and sends it to the correct feature handler.

  How to use:
    Called automatically in loop() after dwinRead() returns true.
*/
void handleDWIN(uint16_t vp, uint16_t value) {
  if (dwinBacklightSleeping) {
    registerUserActivity("DWIN touch wake");

    // First touch while the backlight is off only wakes the display.
    // This prevents an accidental hidden button press.
    if (vp == VP_DISPLAY_WAKE_TOUCH) {
      writeVPWord(VP_DISPLAY_WAKE_TOUCH, 0);
    }
    return;
  }

  registerUserActivity("DWIN touch");

  bool sensitivePINInput =
      vp == VP_ADMIN_PIN_INPUT ||
      (vp == VP_KEYBOARD_CMD &&
       (keyboardTarget == KB_TARGET_ADMIN_PIN ||
        keyboardTarget == KB_TARGET_UPDATE_OLD_PIN ||
        keyboardTarget == KB_TARGET_UPDATE_NEW_PIN ||
        keyboardTarget == KB_TARGET_UPDATE_CONFIRM_PIN));
  Serial.print("Received VP 0x");
  printHex16(vp);
  if (sensitivePINInput) {
    Serial.println(" value ****");
  } else {
    Serial.print(" value 0x");
    printHex16(value);
    Serial.println();
  }

  if (pendingLockerAction.type != LOCKER_ACTION_NONE &&
      vp != VP_SLOT_CONFIRM && vp != VP_SLOT_CANCEL) {
    Serial.println("Input ignored while key confirmation is pending.");
    return;
  }

  if (handleDisplayWakeTouch(vp, value)) return;
  if (handleKeyboard(vp, value)) return;
  if (handleQRControls(vp, value)) return;
  if (handlePageCommand(vp, value)) return;
  if (handleAdminAccess(vp, value)) return;
  if (handleAccountControls(vp, value)) return;
  if (handleUpdatePasswordControls(vp, value)) return;
  if (handleRegistrationControls(vp, value)) return;
  if (handleKeyRegistrationControls(vp, value)) return;
  if (handleMaintenanceControls(vp, value)) return;
  if (handleDateCards(vp, value)) return;
  if (handleTransactionLogControls(vp, value)) return;
  if (handleCurrentLogControls(vp, value)) return;
  if (handleSlotControls(vp, value)) return;

  Serial.println("No handler used this VP.");
}

/*
  Function: dwinRead()
  Description:
    Reads a DWIN UART return frame:
      5A A5 06 83 VP_H VP_L 01 VALUE_H VALUE_L

  How to use:
    uint16_t vp, value;
    if (dwinRead(vp, value)) handleDWIN(vp, value);
*/
bool dwinRead(uint16_t &receivedVP, uint16_t &receivedValue) {
  static uint8_t buffer[128];
  static int index = 0;
  static int expectedLength = -1;

  while (DWIN.available()) {
    uint8_t b = DWIN.read();

    if (index == 0 && b != 0x5A) continue;

    if (index == 1 && b != 0xA5) {
      index = 0;
      expectedLength = -1;
      continue;
    }

    buffer[index++] = b;

    if (index == 3) {
      expectedLength = buffer[2] + 3;
      if (expectedLength > (int)sizeof(buffer)) {
        index = 0;
        expectedLength = -1;
        continue;
      }
    }

    if (expectedLength > 0 && index >= expectedLength) {
      bool validFrame = false;
      dwinReceivedWordCount = 0;
      dwinReceivedSecondWord = 0;

      if (index >= 9 && buffer[3] == 0x83) {
        uint8_t wordCount = buffer[6];
        if (wordCount > 0) {
          uint16_t requiredLength = 9 + ((uint16_t)(wordCount - 1) * 2);
          if (index >= requiredLength) {
            receivedVP = ((uint16_t)buffer[4] << 8) | buffer[5];
            receivedValue = ((uint16_t)buffer[7] << 8) | buffer[8];
            dwinReceivedWordCount = wordCount;
            if (wordCount > 1) {
              dwinReceivedSecondWord = ((uint16_t)buffer[9] << 8) | buffer[10];
            }
            validFrame = true;
          }
        }
      }

      index = 0;
      expectedLength = -1;

      if (validFrame) {
        debugHex16("dwinRead", "Received VP", receivedVP);
        debugHex16("dwinRead", "Received value", receivedValue);
        return true;
      }
    }
  }

  return false;
}

/*
  Function: writeVPWord()
  Description:
    Writes one 16-bit value to a DWIN VP.

  How to use:
    writeVPWord(0x1300, 50);
*/
void writeVPWord(uint16_t vp, uint16_t value) {
  uint8_t frame[] = {
    0x5A, 0xA5, 0x05, 0x82,
    (uint8_t)(vp >> 8), (uint8_t)(vp & 0xFF),
    (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
  };

  DWIN.write(frame, sizeof(frame));
  DWIN.flush();
}

/*
  Function: writeVPText()
  Description:
    Writes text to a DWIN text VP and fills the unused part with spaces.
    This prevents old/random characters from appearing.

  How to use:
    writeVPText(0x1500, "2026-07-09", 16);
*/
void writeVPText(uint16_t vp, const char *text, uint8_t fieldLength) {
  uint8_t len = 3 + fieldLength;

  DWIN.write(0x5A);
  DWIN.write(0xA5);
  DWIN.write(len);
  DWIN.write(0x82);
  DWIN.write((uint8_t)(vp >> 8));
  DWIN.write((uint8_t)(vp & 0xFF));

  bool textEnded = false;

  for (int i = 0; i < fieldLength; i++) {
    if (!textEnded && text[i] != '\0') {
      DWIN.write((uint8_t)text[i]);
    } else {
      textEnded = true;
      DWIN.write(' ');
    }
  }

  DWIN.flush();
}

/*
  Function: changePage()
  Description:
    Changes the DWIN display page.

  How to use:
    changePage(PAGE_ADMIN_DASHBOARD);
*/
void changePage(uint16_t pageID) {
  debugNumber("changePage", "Opening page", pageID);

  // Make sure ESP32-controlled page changes are visible even after
  // the idle timer turns the DWIN backlight off.
  wakeDWINBacklight();

  uint8_t frame[] = {
    0x5A, 0xA5, 0x07, 0x82,
    0x00, 0x84, 0x5A, 0x01,
    (uint8_t)(pageID >> 8), (uint8_t)(pageID & 0xFF)
  };

  DWIN.write(frame, sizeof(frame));
  DWIN.flush();
}


/*
  Function: setDWINBacklight()
  Description:
    Sets the DWIN backlight brightness through the DGUS backlight control VP.
    A brightness of 100 is full brightness. A brightness of 0 turns the
    backlight off, which behaves like a sleep screen while the DWIN logic
    remains powered.

  How to use:
    setDWINBacklight(100);  // wake / full brightness
    setDWINBacklight(0);    // backlight off
*/
void setDWINBacklight(uint8_t brightness) {
  if (brightness > 100) brightness = 100;

  // Many DWIN DGUS/T5L displays use the high byte as active brightness
  // and the low byte as standby brightness. Writing both the same value
  // makes wake/sleep behavior more consistent.
  uint16_t value = ((uint16_t)brightness << 8) | brightness;
  writeVPWord(DWIN_BACKLIGHT_VP, value);
}

/*
  Function: sleepDWINBacklight()
  Description:
    Turns off the DWIN backlight without powering off the DWIN display.
    UART remains available so ESP32 can wake it again.

  How to use:
    Call sleepDWINBacklight(); after the device is idle.
*/
void sleepDWINBacklight() {
  setDWINBacklight(DWIN_BACKLIGHT_OFF);
  dwinBacklightSleeping = true;
}

/*
  Function: wakeDWINBacklight()
  Description:
    Restores DWIN backlight brightness before showing a page or important data.

  How to use:
    Call wakeDWINBacklight(); before changePage(), or let changePage() call it.
*/
void wakeDWINBacklight() {
  setDWINBacklight(DWIN_BACKLIGHT_FULL);
  dwinBacklightSleeping = false;
  delay(80);
}

/*
  Function: printHex16()
  Description:
    Prints a 16-bit value as 4-digit HEX.

  How to use:
    printHex16(0x1044);
*/
void printHex16(uint16_t value) {
  if (value < 0x1000) Serial.print("0");
  if (value < 0x0100) Serial.print("0");
  if (value < 0x0010) Serial.print("0");
  Serial.print(value, HEX);
}
