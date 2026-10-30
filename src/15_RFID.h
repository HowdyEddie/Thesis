// =======================================================
// RFID.ino
// RC522 RFID over SPI.
// =======================================================

/*
  Function: initRFIDModule()
  Description:
    Starts the RC522 RFID reader.

  How to use:
    Called during loading screen.
*/
bool initRFIDModule() {
  debugLog("initRFIDModule", "Starting RC522 initialization.");

  MFRC522::MIFARE_Key key;
  rfid.PCD_Init();

  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  debugHex16("initRFIDModule", "RC522 VersionReg", version);

  if (version == 0x00 || version == 0xFF) {
    rfidReady = false;
    debugFail("initRFIDModule", "RFID RC522 not detected. Check SPI wiring, CS GPIO5, RST GPIO4, and power.");
    return false;
  }

  rfidReady = true;
  debugOK("initRFIDModule");
  return true;
}

/*
  Function: readRFIDTagOnce()
  Description:
    Reads one RFID UID if a card is present.

  How to use:
    String uid = readRFIDTagOnce();
*/
String readRFIDTagOnce() {
  if (!rfidReady) {
    debugFail("readRFIDTagOnce", "RFID module not ready.");
    return "";
  }

  if (!rfid.PICC_IsNewCardPresent()) return "";
  if (!rfid.PICC_ReadCardSerial()) {
    debugFail("readRFIDTagOnce", "Card is present but UID read failed.");
    return "";
  }

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  registerUserActivity("RFID");
  debugLog("readRFIDTagOnce", "RFID tag read.");

  return uid;
}


/*
  Function: waitForRFIDTag()
  Description:
    Waits for one RFID UID until timeout.

  How to use:
    String uid = waitForRFIDTag(10000);
*/
String waitForRFIDTag(uint32_t timeoutMs) {
  unsigned long startTime = millis();

  while (millis() - startTime < timeoutMs) {
    String uid = readRFIDTagOnce();

    if (uid.length() > 0) {
      return normalizeRFIDUID(uid);
    }

    delayWithUnlockService(100);
  }

  return "";
}

/*
  Function: checkLockedScreenRFIDReturn()
  Description:
    Automatically scans RFID on Home Dashboard Pages 2/3 only. A matching
    borrowed key stages a return and opens its slot until dashboard OK confirms.

  How to use:
    Called in loop() while Home Dashboard Page 2 or 3 is active.
*/
void checkLockedScreenRFIDReturn() {
  if (!rfidReady) return;
  if (pendingLockerAction.type != LOCKER_ACTION_NONE) return;
  if (digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL) return;
  if (millis() - lastRFIDReturnCheck < RFID_RETURN_SCAN_INTERVAL) return;

  lastRFIDReturnCheck = millis();

  String uid = readRFIDTagOnce();
  uid = normalizeRFIDUID(uid);

  if (uid.length() == 0) return;

  if (uid == lastReturnedRFIDUID && millis() - lastRFIDReturnAction < RFID_RETURN_REPEAT_GUARD_MS) {
    return;
  }

  int keyIndex = findRegisteredKeyIndexByRFID(uid);

  if (keyIndex < 0) {
    debugLog("checkLockedScreenRFIDReturn", "Unregistered RFID tag ignored.");
    return;
  }

  uint8_t slotNumber = registeredKeys[keyIndex].slotNumber;
  if (slotNumber < 1 || slotNumber > SLOT_COUNT || !slotTaken[slotNumber - 1]) {
    debugLog("checkLockedScreenRFIDReturn", "Registered RFID key is not currently borrowed.");
    return;
  }

  beginRegisteredKeyReturn((uint8_t)keyIndex);
}

bool beginRegisteredKeyReturn(uint8_t keyIndex) {
  if (pendingLockerAction.type != LOCKER_ACTION_NONE ||
      keyIndex >= MAX_KEYS || !registeredKeys[keyIndex].used) {
    return false;
  }

  uint8_t slotNumber = registeredKeys[keyIndex].slotNumber;
  if (slotNumber < 1 || slotNumber > SLOT_COUNT) return false;
  uint8_t slotIndex = slotNumber - 1;
  if (!slotTaken[slotIndex] ||
      digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL) {
    return false;
  }

  pendingLockerAction.type = LOCKER_ACTION_RETURN;
  pendingLockerAction.slotIndex = slotIndex;
  pendingLockerAction.keyIndex = keyIndex;
  pendingLockerAction.keyNumber = registeredKeys[keyIndex].keyNumber;
  pendingLockerAction.keyName = registeredKeys[keyIndex].keyName;
  pendingLockerAction.borrowerName = "";
  pendingLockerAction.studentCode = "";
  pendingLockerAction.userName = "RFID Return";

  clearSlotSelections();
  selectedSlotIndex = -1;
  slotUnlockActive[slotIndex] = false;
  setSlotLockOutput(slotIndex, true);
  showKeyConfirmationPopup();
  debugNumber("beginRegisteredKeyReturn", "Waiting for return confirmation for slot", slotNumber);
  return true;
}

/*
  Function: markRegisteredKeyReturned()
  Description:
    Commits a confirmed RFID return, updates logs and saved slot state, then
    closes the slot output.

  How to use:
    markRegisteredKeyReturned(keyIndex);
*/
bool markRegisteredKeyReturned(uint8_t keyIndex) {
  if (keyIndex >= MAX_KEYS || !registeredKeys[keyIndex].used) return false;

  uint8_t slotNumber = registeredKeys[keyIndex].slotNumber;
  if (slotNumber < 1 || slotNumber > SLOT_COUNT) return false;

  uint8_t slotIndex = slotNumber - 1;

  String returnedTime = getRTCTimeString();
  String today = getRTCDateString();

  if (!addTransactionLog(
          registeredKeys[keyIndex].keyNumber,
          "Returned",
          registeredKeys[keyIndex].keyName,
          "--:--",
          returnedTime,
          "RFID Return",
          today)) {
    debugFail("markRegisteredKeyReturned", "Return cancelled because the daily transaction could not be committed.");
    return false;
  }

  addCurrentLog(
    registeredKeys[keyIndex].keyName,
    "Returned",
    registeredKeys[keyIndex].keyNumber,
    returnedTime,
    LOG_STATUS_RETURNED
  );

  slotTaken[slotIndex] = false;
  saveSlotTakenState(slotIndex);
  slotDisplayState[slotIndex] = SLOT_AVAILABLE;
  slotSelected[slotIndex] = false;
  slotUnlockActive[slotIndex] = false;
  setSlotLockOutput(slotIndex, false);
  updateSlotDisplay(slotIndex);

  String line = today + "," + returnedTime + "," +
                String(slotNumber) + "," +
                csvEscapeField(registeredKeys[keyIndex].keyName) + "," +
                csvEscapeField(registeredKeys[keyIndex].keyNumber) + "," +
                csvEscapeField(normalizeRFIDUID(registeredKeys[keyIndex].rfidUID));

  if (!appendLineToDaySD("/returns.csv", line)) {
    debugFail("markRegisteredKeyReturned", "Transaction is committed and slot returned, but the auxiliary returns.csv row was not saved.");
  }

  debugNumber("markRegisteredKeyReturned", "Returned slot", slotNumber);
  return true;
}

/*
  Function: updateReturnUnlockRelock()
  Description:
    Services any legacy timed slot unlocks. Confirmed borrow/return actions
    now close their output directly after the transaction commits.

  How to use:
    Called continuously in loop().
*/
void updateReturnUnlockRelock() {
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    if (!slotUnlockActive[i]) continue;
    if (millis() - slotUnlockStartTime[i] < RETURN_UNLOCK_MS) continue;

    setSlotLockOutput(i, false);
    slotUnlockActive[i] = false;
    debugNumber("updateReturnUnlockRelock", "Relocked slot", i + 1);
  }
}

void startTimedSlotUnlock(uint8_t slotIndex) {
  if (slotIndex >= SLOT_COUNT) return;
  slotUnlockActive[slotIndex] = true;
  slotUnlockStartTime[slotIndex] = millis();
}

void delayWithUnlockService(uint32_t durationMs) {
  unsigned long startTime = millis();
  while (millis() - startTime < durationMs) {
    updateReturnUnlockRelock();
    delay(1);
  }
}
