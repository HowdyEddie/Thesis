// =======================================================
// KeyRegistration.ino
// Page 10 key registration linked to slot number and RFID UID.
// =======================================================

/*
  Function: handleKeyRegistrationControls()
  Description:
    Handles Page 10 new key registration controls.

    Final flow:
      1. Type Key Name.
      2. Type Key Number / Slot Number from 1 to 40.
      3. Press OK.
      4. ESP32 asks for RFID scan automatically.
      5. Key record is saved after a valid RFID UID is read.

  How to use:
    Key Name button   -> type key name.
    Key Number button -> type slot/key number 1 to 40.
    OK button         -> validate data, scan RFID, save key.
*/
bool handleKeyRegistrationControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_NEW_KEY_REG) return false;

  if (vp == VP_KEYREG_KEY_NAME_BUTTON) {
    writeVPWord(VP_KEYREG_KEY_NAME_BUTTON, 0);
    openKeyboard(KB_TARGET_KEY_NAME, PAGE_NEW_KEY_REG, false);
    return true;
  }

  if (vp == VP_KEYREG_KEY_NO_BUTTON) {
    writeVPWord(VP_KEYREG_KEY_NO_BUTTON, 0);
    openKeyboard(KB_TARGET_KEY_NUMBER, PAGE_NEW_KEY_REG, true);
    return true;
  }

  if (vp == VP_KEYREG_OK) {
    writeVPWord(VP_KEYREG_OK, 0);
    saveNewKeyRegistration();
    return true;
  }

  if (vp == VP_KEYREG_BACK) {
    writeVPWord(VP_KEYREG_BACK, 0);
    goToRegistrationPage();
    return true;
  }

  return false;
}

/*
  Function: renderNewKeyRegistrationPage()
  Description:
    Displays key name, slot/key number, RFID UID, and status.

  How to use:
    Called when Page 10 opens or after keyboard/RFID updates.
*/
void renderNewKeyRegistrationPage() {
  writeVPText(VP_KEYREG_KEY_NAME_TEXT, pendingKeyName.c_str(), 32);
  writeVPText(VP_KEYREG_KEY_NO_TEXT, pendingKeyNumber.c_str(), 16);
  writeVPText(VP_KEYREG_RFID_TEXT, pendingKeyRFID.c_str(), KEYREG_RFID_BYTES);

  if (pendingKeyName.length() == 0) {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Enter key name", 32);
  }
  else if (pendingKeyNumber.length() == 0) {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Enter slot 1-40", 32);
  }
  else {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Press OK to scan", 32);
  }

  writeVPWord(VP_KEYREG_STATUS_ICON, ICON_IDLE);
}

/*
  Function: parseSlotNumberFromKeyNumber()
  Description:
    Converts the Page 10 Key Number text into a valid slot number.
    Valid values are 1 to 40. Any value above 40 is rejected.

  How to use:
    uint8_t slotNumber = parseSlotNumberFromKeyNumber(pendingKeyNumber);
*/
uint8_t parseSlotNumberFromKeyNumber(String keyNumberText) {
  keyNumberText.trim();

  if (keyNumberText.length() == 0) return 0;

  for (uint16_t i = 0; i < keyNumberText.length(); i++) {
    if (!isDigit(keyNumberText.charAt(i))) return 0;
  }

  int slotNumber = keyNumberText.toInt();

  if (slotNumber < 1 || slotNumber > SLOT_COUNT) return 0;

  return (uint8_t)slotNumber;
}

/*
  Function: normalizeRFIDUID()
  Description:
    Cleans an RFID UID so saved UID and scanned UID can match reliably.

  How to use:
    String cleanUID = normalizeRFIDUID(rawUID);
*/
String normalizeRFIDUID(String rfidUID) {
  rfidUID.trim();
  rfidUID.replace(" ", "");
  rfidUID.replace(":", "");
  rfidUID.replace("-", "");
  rfidUID.toUpperCase();

  return rfidUID;
}

/*
  Function: findRegisteredKeyIndexBySlot()
  Description:
    Finds the key record index for a slot number.

  How to use:
    int index = findRegisteredKeyIndexBySlot(5);
*/
int findRegisteredKeyIndexBySlot(uint8_t slotNumber) {
  for (uint8_t i = 0; i < MAX_KEYS; i++) {
    if (registeredKeys[i].used && registeredKeys[i].slotNumber == slotNumber) {
      return i;
    }
  }

  return -1;
}

/*
  Function: findRegisteredKeyIndexByRFID()
  Description:
    Finds the key record index for a scanned RFID UID.

  How to use:
    int index = findRegisteredKeyIndexByRFID("04A1B2C3");
*/
int findRegisteredKeyIndexByRFID(String rfidUID) {
  rfidUID = normalizeRFIDUID(rfidUID);

  if (rfidUID.length() == 0) return -1;

  for (uint8_t i = 0; i < MAX_KEYS; i++) {
    if (registeredKeys[i].used && normalizeRFIDUID(registeredKeys[i].rfidUID) == rfidUID) {
      return i;
    }
  }

  return -1;
}

/*
  Function: upsertRegisteredKey()
  Description:
    Adds or updates one key record in RAM.

  How to use:
    upsertRegisteredKey(1, "Science Department", "1", "04A1B2C3");
*/
bool upsertRegisteredKey(uint8_t slotNumber, String keyName, String keyNumber, String rfidUID) {
  keyName.trim();
  keyNumber.trim();
  rfidUID = normalizeRFIDUID(rfidUID);

  if (slotNumber < 1 || slotNumber > SLOT_COUNT) return false;
  if (keyName.length() == 0 || keyNumber.length() == 0 || rfidUID.length() == 0) return false;

  int existingRFIDIndex = findRegisteredKeyIndexByRFID(rfidUID);
  if (existingRFIDIndex >= 0 && registeredKeys[existingRFIDIndex].slotNumber != slotNumber) {
    debugFail("upsertRegisteredKey", "RFID UID is already assigned to another slot.");
    return false;
  }

  int existingIndex = findRegisteredKeyIndexBySlot(slotNumber);

  if (existingIndex >= 0) {
    registeredKeys[existingIndex].slotNumber = slotNumber;
    registeredKeys[existingIndex].keyName = keyName;
    registeredKeys[existingIndex].keyNumber = keyNumber;
    registeredKeys[existingIndex].rfidUID = rfidUID;
    registeredKeys[existingIndex].used = true;
    debugNumber("upsertRegisteredKey", "Updated slot", slotNumber);
    return true;
  }

  for (uint8_t i = 0; i < MAX_KEYS; i++) {
    if (!registeredKeys[i].used) {
      registeredKeys[i].slotNumber = slotNumber;
      registeredKeys[i].keyName = keyName;
      registeredKeys[i].keyNumber = keyNumber;
      registeredKeys[i].rfidUID = rfidUID;
      registeredKeys[i].used = true;
      registeredKeyCount++;
      debugNumber("upsertRegisteredKey", "Added slot", slotNumber);
      return true;
    }
  }

  debugFail("upsertRegisteredKey", "Key list is full.");
  return false;
}

/*
  Function: getRegisteredKeyForSlot()
  Description:
    Gets the registered key record for a selected slot index.

  How to use:
    KeyEntry key;
    if (getRegisteredKeyForSlot(selectedSlotIndex, key)) { ... }
*/
bool getRegisteredKeyForSlot(uint8_t slotIndex, KeyEntry &keyOut) {
  uint8_t slotNumber = slotIndex + 1;
  int index = findRegisteredKeyIndexBySlot(slotNumber);

  if (index < 0) return false;

  keyOut = registeredKeys[index];
  return true;
}

/*
  Function: saveNewKeyRegistration()
  Description:
    Validates key name and slot number, asks for RFID automatically,
    then saves the linked key record.

    Saved link:
      slot number + key name + key number + RFID UID

  How to use:
    On Page 10, type Key Name and Key Number, then press OK.
*/
void saveNewKeyRegistration() {
  pendingKeyName.trim();
  pendingKeyNumber.trim();

  if (pendingKeyName.length() == 0) {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Enter key name", 32);
    writeVPWord(VP_KEYREG_STATUS_ICON, ICON_KEY_FAILED);
    debugFail("saveNewKeyRegistration", "Missing key name.");
    return;
  }

  uint8_t slotNumber = parseSlotNumberFromKeyNumber(pendingKeyNumber);

  if (slotNumber == 0) {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Slot must be 1-40", 32);
    writeVPWord(VP_KEYREG_STATUS_ICON, ICON_KEY_FAILED);
    debugFail("saveNewKeyRegistration", "Key number / slot number must be 1 to 40.");
    return;
  }

  writeVPText(VP_KEYREG_STATUS_TEXT, "Scan RFID tag", 32);
  writeVPWord(VP_KEYREG_STATUS_ICON, ICON_SCAN_RFID);

  String uid = waitForRFIDTag(KEYREG_RFID_WAIT_MS);
  uid = normalizeRFIDUID(uid);

  if (uid.length() == 0) {
    pendingKeyRFID = "";
    writeVPText(VP_KEYREG_RFID_TEXT, "", KEYREG_RFID_BYTES);
    writeVPText(VP_KEYREG_STATUS_TEXT, "RFID timeout", 32);
    writeVPWord(VP_KEYREG_STATUS_ICON, ICON_KEY_FAILED);
    debugFail("saveNewKeyRegistration", "No RFID tag scanned before timeout.");
    return;
  }

  pendingKeyRFID = uid;
  writeVPText(VP_KEYREG_RFID_TEXT, pendingKeyRFID.c_str(), KEYREG_RFID_BYTES);

  bool wasUpdate = findRegisteredKeyIndexBySlot(slotNumber) >= 0;

  if (!upsertRegisteredKey(slotNumber, pendingKeyName, pendingKeyNumber, pendingKeyRFID)) {
    writeVPText(VP_KEYREG_STATUS_TEXT, "Key save failed", 32);
    writeVPWord(VP_KEYREG_STATUS_ICON, ICON_KEY_FAILED);
    return;
  }

  saveKeysToPreferences();

  String actionText = wasUpdate ? "update" : "new";
  String line = getRTCDateTimeString() + "," +
                String(slotNumber) + "," +
                csvEscapeField(pendingKeyName) + "," +
                csvEscapeField(pendingKeyNumber) + "," +
                csvEscapeField(pendingKeyRFID) + "," +
                actionText;

  appendLineToDaySD("/keys.csv", line);

  writeVPText(VP_KEYREG_STATUS_TEXT, "Key saved", 32);
  writeVPWord(VP_KEYREG_STATUS_ICON, ICON_KEY_SAVED);

  debugNumber("saveNewKeyRegistration", "Registered slot", slotNumber);

  renderHomeSlotKeyNames();
}

/*
  Function: renderHomeSlotKeyNames()
  Description:
    Writes the registered key name for each slot to the Home Dashboard
    slot-name text VPs. Empty slots are written as blank.

  How to use:
    Called when Home Dashboard opens and after key registration changes.
*/
void renderHomeSlotKeyNames() {
  for (uint8_t slotIndex = 0; slotIndex < SLOT_COUNT; slotIndex++) {
    uint16_t vp = VP_SLOT_KEY_NAME_BASE + (slotIndex * SLOT_KEY_NAME_STEP);

    KeyEntry key;
    if (getRegisteredKeyForSlot(slotIndex, key)) {
      writeVPText(vp, key.keyName.c_str(), SLOT_KEY_NAME_BYTES);
    } else {
      writeVPText(vp, "", SLOT_KEY_NAME_BYTES);
    }
  }
}

/*
  Function: clearHomeSlotKeyNames()
  Description:
    Clears all Home Dashboard slot-name text VPs.

  How to use:
    Called by resetEverythingForActualProject().
*/
void clearHomeSlotKeyNames() {
  for (uint8_t slotIndex = 0; slotIndex < SLOT_COUNT; slotIndex++) {
    uint16_t vp = VP_SLOT_KEY_NAME_BASE + (slotIndex * SLOT_KEY_NAME_STEP);
    writeVPText(vp, "", SLOT_KEY_NAME_BYTES);
  }
}

/*
  Function: loadKeysFromPreferences()
  Description:
    Loads registered key records from ESP32 Preferences.

  How to use:
    Called during loading screen.
*/
void loadKeysFromPreferences() {
  clearRegisteredKeysInRAM();

  prefs.begin("keys", true);

  uint8_t savedCount = prefs.getUChar("count", 0);
  if (savedCount > MAX_KEYS) savedCount = MAX_KEYS;

  uint8_t loadedCount = 0;

  for (uint8_t i = 0; i < savedCount; i++) {
    String prefix = "k" + String(i) + "_";

    uint8_t slotNumber = prefs.getUChar((prefix + "slot").c_str(), 0);
    String keyName = prefs.getString((prefix + "name").c_str(), "");
    String keyNumber = prefs.getString((prefix + "no").c_str(), "");
    String rfidUID = prefs.getString((prefix + "rfid").c_str(), "");

    if (slotNumber >= 1 && slotNumber <= SLOT_COUNT && keyName.length() > 0) {
      registeredKeys[loadedCount].slotNumber = slotNumber;
      registeredKeys[loadedCount].keyName = keyName;
      registeredKeys[loadedCount].keyNumber = keyNumber.length() > 0 ? keyNumber : String(slotNumber);
      registeredKeys[loadedCount].rfidUID = normalizeRFIDUID(rfidUID);
      registeredKeys[loadedCount].used = true;
      loadedCount++;
    }
  }

  prefs.end();

  registeredKeyCount = loadedCount;
  debugNumber("loadKeysFromPreferences", "Registered keys loaded", registeredKeyCount);
}

/*
  Function: saveKeysToPreferences()
  Description:
    Saves all registered key records to ESP32 Preferences.

  How to use:
    Called after saving or updating a key registration.
*/
void saveKeysToPreferences() {
  prefs.begin("keys", false);
  prefs.clear();

  uint8_t saveIndex = 0;

  for (uint8_t i = 0; i < MAX_KEYS; i++) {
    if (!registeredKeys[i].used) continue;

    String prefix = "k" + String(saveIndex) + "_";

    prefs.putUChar((prefix + "slot").c_str(), registeredKeys[i].slotNumber);
    prefs.putString((prefix + "name").c_str(), registeredKeys[i].keyName);
    prefs.putString((prefix + "no").c_str(), registeredKeys[i].keyNumber);
    prefs.putString((prefix + "rfid").c_str(), normalizeRFIDUID(registeredKeys[i].rfidUID));

    saveIndex++;
  }

  prefs.putUChar("count", saveIndex);
  prefs.end();

  registeredKeyCount = saveIndex;
  debugNumber("saveKeysToPreferences", "Registered keys saved", registeredKeyCount);
}

/*
  Function: clearRegisteredKeysInRAM()
  Description:
    Clears all key records from RAM.

  How to use:
    Called by loadKeysFromPreferences() and resetEverythingForActualProject().
*/
void clearRegisteredKeysInRAM() {
  for (uint8_t i = 0; i < MAX_KEYS; i++) {
    registeredKeys[i].slotNumber = 0;
    registeredKeys[i].keyName = "";
    registeredKeys[i].keyNumber = "";
    registeredKeys[i].rfidUID = "";
    registeredKeys[i].used = false;
  }

  registeredKeyCount = 0;
}
