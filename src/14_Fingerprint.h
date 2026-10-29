// =======================================================
// Fingerprint.ino
// R503 capacitive FPS using Adafruit Fingerprint library.
// =======================================================

/*
  Function: initFingerprintModule()
  Description:
    Starts the R503 fingerprint module on UART1 and reads sensor parameters.

  How to use:
    Called during loading screen.
*/
bool initFingerprintModule() {
  FPSSerial.begin(FPS_BAUD, SERIAL_8N1, FPS_RX, FPS_TX);
  finger.begin(FPS_BAUD);

  if (finger.verifyPassword()) {
    fpsReady = true;
    debugOK("initFingerprintModule");

    uint8_t paramResult = finger.getParameters();

    if (paramResult == FINGERPRINT_OK) {
      debugNumber("initFingerprintModule", "Sensor capacity", finger.capacity);
      debugNumber("initFingerprintModule", "Security level", finger.security_level);
      debugNumber("initFingerprintModule", "Baud rate", finger.baud_rate);
      debugNumber("initFingerprintModule", "Packet length", finger.packet_len);
    } else {
      debugNumber("initFingerprintModule", "getParameters failed. Error", paramResult);
    }

    uint8_t p = finger.getTemplateCount();

    if (p == FINGERPRINT_OK) {
      debugNumber("initFingerprintModule", "Fingerprint templates stored in sensor", finger.templateCount);
    } else {
      debugNumber("initFingerprintModule", "Could not read template count. Error", p);
    }

    return true;
  }

  fpsReady = false;
  debugFail("initFingerprintModule", "Fingerprint sensor not found. Check TX/RX, power, baud.");
  return false;
}

/*
  Function: checkLockedScreenFingerprint()
  Description:
    Checks fingerprint on Page 1 Locked Screen.
    Only a fingerprint ID linked to a saved account can open the dashboard.

  How to use:
    Called automatically in loop() when currentPage == PAGE_LOCKED_SCREEN.
*/
void checkLockedScreenFingerprint() {
  if (millis() - lastFingerprintCheck < 500) return;

  lastFingerprintCheck = millis();

  if (!fpsReady) {
    static unsigned long lastFingerprintRetry = 0;
    if (millis() - lastFingerprintRetry < 5000) return;

    lastFingerprintRetry = millis();
    debugLog("checkLockedScreenFingerprint", "Fingerprint sensor is not ready; retrying initialization.");
    fpsReady = initFingerprintModule();
    return;
  }

  int id = getFingerprintID();

  if (id > 0) {
    if (isFingerprintIDRegistered(id)) {
      debugLog("checkLockedScreenFingerprint", "Registered fingerprint matched.");
      setCurrentHomeUserFromFingerprint(id);
      goToHomeDashboard(PAGE_HOME_DASHBOARD_A);
    } else {
      debugLog("checkLockedScreenFingerprint", "Unlinked fingerprint template matched.");
    }
  }
}

/*
  Function: getFingerprintID()
  Description:
    Captures a fingerprint and searches the sensor database.
    Uses normal fingerSearch(1) instead of fingerFastSearch() because some
    R503 sensors return non-standard error 252 during fast search.

  How to use:
    int id = getFingerprintID();
*/
int getFingerprintID() {
  uint8_t p = finger.getImage();

  if (p == FINGERPRINT_NOFINGER) return -1;

  if (p != FINGERPRINT_OK) {
    debugNumber("getFingerprintID", "getImage failed. Error", p);
    return -1;
  }

  registerUserActivity("Fingerprint");
  debugLog("getFingerprintID", "Fingerprint image captured.");

  p = finger.image2Tz(1);

  if (p != FINGERPRINT_OK) {
    debugNumber("getFingerprintID", "image2Tz failed. Error", p);
    return -1;
  }

  debugLog("getFingerprintID", "Fingerprint image converted.");

  p = finger.fingerSearch(1);

  if (p == FINGERPRINT_OK) {
    debugLog("getFingerprintID", "Fingerprint matched.");
    return finger.fingerID;
  }

  if (p == FINGERPRINT_NOTFOUND) {
    debugFail("getFingerprintID", "No matching fingerprint found.");
    return -1;
  }

  debugNumber("getFingerprintID", "fingerSearch failed. Error", p);
  return -1;
}

/*
  Function: findFreeFingerprintID()
  Description:
    Finds a free fingerprint ID by checking saved accounts and sensor storage.

  How to use:
    uint16_t id = findFreeFingerprintID();
*/
uint16_t findFreeFingerprintID() {
  if (!fpsReady) {
    debugFail("findFreeFingerprintID", "FPS is not ready.");
    return 0;
  }

  finger.getTemplateCount();
  debugNumber("findFreeFingerprintID", "Template count before registration", finger.templateCount);

  for (uint16_t id = 1; id <= 200; id++) {
    if (isFingerprintIDRegistered(id)) continue;

    uint8_t p = finger.loadModel(id);
    if (p == FINGERPRINT_OK) continue;

    debugLog("findFreeFingerprintID", "Free fingerprint template slot selected.");
    return id;
  }

  debugFail("findFreeFingerprintID", "No free ID from 1 to 200.");
  return 0;
}

/*
  Function: waitForFingerImage()
  Description:
    Waits for a finger image or timeout.

  How to use:
    int result = waitForFingerImage(15000);
*/
int waitForFingerImage(uint32_t timeoutMs) {
  unsigned long startTime = millis();

  while (millis() - startTime < timeoutMs) {
    updateReturnUnlockRelock();
    int p = finger.getImage();

    if (p == FINGERPRINT_OK) return p;

    if (p != FINGERPRINT_NOFINGER) {
      debugNumber("waitForFingerImage", "getImage waiting error", p);
    }

    delayWithUnlockService(80);
  }

  return -1;
}

/*
  Function: waitForFingerRelease()
  Description:
    Waits until the user removes the finger.

  How to use:
    bool released = waitForFingerRelease(10000);
*/
bool waitForFingerRelease(uint32_t timeoutMs) {
  unsigned long startTime = millis();

  while (millis() - startTime < timeoutMs) {
    updateReturnUnlockRelock();
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_NOFINGER) return true;

    delayWithUnlockService(80);
  }

  return false;
}

/*
  Function: verifyStoredFingerprintID()
  Description:
    Confirms that a stored template can be loaded from the requested ID.

  How to use:
    bool stored = verifyStoredFingerprintID(id);
*/
bool verifyStoredFingerprintID(uint16_t id) {
  uint8_t p = finger.loadModel(id);

  if (p == FINGERPRINT_OK) {
    debugLog("verifyStoredFingerprintID", "Stored fingerprint template verified.");
    return true;
  }

  debugNumber("verifyStoredFingerprintID", "loadModel failed. Error", p);
  return false;
}

/*
  Function: enrollFingerprintBlocking()
  Description:
    Enrolls one fingerprint, stores it, then verifies the stored template
    with loadModel(id). This is safer for R503 modules that may return an
    unusual storeModel result.

  How to use:
    enrollFingerprintBlocking(5);
*/
bool enrollFingerprintBlocking(uint16_t id) {
  int p;

  writeVPText(VP_REGISTER_STATUS_TEXT, "Place finger", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_PLACE_FINGER);
  debugLog("enrollFingerprintBlocking", "Step 1: place finger.");

  p = waitForFingerImage(15000);

  if (p != FINGERPRINT_OK) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Finger timeout", 32);
    debugFail("enrollFingerprintBlocking", "First finger timeout.");
    return false;
  }

  p = finger.image2Tz(1);

  if (p != FINGERPRINT_OK) {
    debugNumber("enrollFingerprintBlocking", "image2Tz(1) failed. Error", p);
    return false;
  }

  writeVPText(VP_REGISTER_STATUS_TEXT, "Remove finger", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_REMOVE_FINGER);
  debugLog("enrollFingerprintBlocking", "Step 2: remove finger.");

  if (!waitForFingerRelease(10000)) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Remove timeout", 32);
    debugFail("enrollFingerprintBlocking", "Finger was not removed.");
    return false;
  }

  delayWithUnlockService(500);

  writeVPText(VP_REGISTER_STATUS_TEXT, "Place again", 32);
  writeVPWord(VP_REGISTER_STATUS_ICON, REG_PLACE_AGAIN);
  debugLog("enrollFingerprintBlocking", "Step 3: place same finger again.");

  p = waitForFingerImage(15000);

  if (p != FINGERPRINT_OK) {
    writeVPText(VP_REGISTER_STATUS_TEXT, "Second timeout", 32);
    debugFail("enrollFingerprintBlocking", "Second finger timeout.");
    return false;
  }

  p = finger.image2Tz(2);

  if (p != FINGERPRINT_OK) {
    debugNumber("enrollFingerprintBlocking", "image2Tz(2) failed. Error", p);
    return false;
  }

  p = finger.createModel();

  if (p != FINGERPRINT_OK) {
    debugNumber("enrollFingerprintBlocking", "createModel() failed. Error", p);

    if (p == FINGERPRINT_ENROLLMISMATCH) {
      writeVPText(VP_REGISTER_STATUS_TEXT, "Finger mismatch", 32);
    }

    return false;
  }

  debugLog("enrollFingerprintBlocking", "Template model created.");

  p = finger.storeModel(id);
  debugNumber("enrollFingerprintBlocking", "storeModel result", p);

  bool verified = verifyStoredFingerprintID(id);

  if (p == FINGERPRINT_OK || verified) {
    debugLog("enrollFingerprintBlocking", "Fingerprint template stored and verified.");
    return true;
  }

  debugNumber("enrollFingerprintBlocking", "storeModel failed. Error", p);
  return false;
}
