// =======================================================
// QR.ino
// GM861S/B1 QR scanner and Page 4 borrower identification.
// =======================================================

/*
  Function: initQRScanner()
  Description:
    Starts the GM861S/B1 QR scanner.

  How to use:
    Called during loading screen.
*/
bool initQRScanner() {
#if HAS_QR_SOFTWARE_SERIAL
  QRSerial.begin(QR_BAUD);
  qrReady = true;
  debugOK("initQRScanner");
#else
  qrReady = false;
  debugFail("initQRScanner", "SoftwareSerial library not found. Install EspSoftwareSerial.");
#endif

  return qrReady;
}

/*
  Function: handleQRScanner()
  Description:
    Reads QR scanner data while Page 4 is open and qrScanActive is true.
    QR scanning now starts automatically when Page 4 opens.

  How to use:
    Called automatically in loop() while Page 4 is open and QR sensing is active.
*/
void handleQRScanner() {
  if (!qrReady || !qrScanActive || currentPage != PAGE_QR_BORROWER) return;

#if HAS_QR_SOFTWARE_SERIAL
  while (qrScanActive && QRSerial.available()) {
    char c = QRSerial.read();

    if (c == '\n' || c == '\r') {
      if (qrBuffer.length() > 0) {
        debugNumber("handleQRScanner", "QR data bytes received", qrBuffer.length());
        processQRData(qrBuffer);
        qrBuffer = "";
      }
    } else {
      qrBuffer += c;
      lastQRByteTime = millis();
    }
  }

  // Some QR scanners do not send ENTER/newline.
  // If characters were received and then stopped, process the buffered QR text.
  if (qrScanActive && qrBuffer.length() > 0 &&
      millis() - lastQRByteTime >= QR_INTERBYTE_TIMEOUT_MS) {
    debugNumber("handleQRScanner", "QR data bytes received before timeout", qrBuffer.length());
    processQRData(qrBuffer);
    qrBuffer = "";
  }
#endif
}

/*
  Function: handleQRControls()
  Description:
    Handles Page 4 QR/Borrower buttons:
      - Manual name input
      - Optional re-scan
      - Cancel/back

    The OK/Confirm button uses VP_SLOT_CONFIRM 0x1028 and is handled
    inside handleSlotControls() because the same OK button is also used
    after selecting a slot.

  How to use:
    DGUS Page 4 buttons:
      VP 0x1640 value 0x0001 = manual name keyboard
      VP 0x1641 value 0x0001 = optional re-scan
      VP 0x1642 value 0x0001 = cancel/back
*/
bool handleQRControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_QR_BORROWER) return false;

  if (vp == VP_QR_NAME_BUTTON) {
    writeVPWord(VP_QR_NAME_BUTTON, 0);

    // Manual typing temporarily pauses QR reading so the scanner cannot overwrite typed text.
    qrScanActive = false;
    qrBuffer = "";
    pendingStudentCode = "";
    openKeyboard(KB_TARGET_BORROWER_NAME, PAGE_QR_BORROWER, false);

    return true;
  }

  if (vp == VP_QR_SCAN_BUTTON) {
    // This button is no longer required.
    // It now works only as an optional Re-scan button.
    writeVPWord(VP_QR_SCAN_BUTTON, 0);

    pendingBorrowerName = "";
    pendingStudentCode = "";
    pendingQRRawData = "";
    qrBuffer = "";
    lastQRByteTime = 0;

    renderQRBorrowerPage();
    startQRScanForBorrower();
    return true;
  }

  if (vp == VP_QR_CANCEL) {
    writeVPWord(VP_QR_CANCEL, 0);
    cancelQRBorrowerPage();
    return true;
  }

  return false;
}

/*
  Function: startQRScanForBorrower()
  Description:
    Activates the QR scanner on Page 4.

    Supported QR text formats:
      Teacher: ALJHUN SILVESTRE 9850
      Student: TUPC-23-0464 DEFANTE DHAN BET-ESET 5916

  How to use:
    Called automatically when Page 4 opens. The QR Scan button is now optional re-scan only.
*/
void startQRScanForBorrower() {
  debugLog("startQRScanForBorrower", "Starting automatic QR sensing on Page 4.");

  if (!qrReady) {
    qrScanActive = false;
    writeVPText(VP_QR_STATUS_TEXT, "QR scanner not ready", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_ERROR);
    debugFail("startQRScanForBorrower", "QR scanner not ready.");
    return;
  }

  qrBuffer = "";
  lastQRByteTime = 0;
  qrScanActive = true;

  writeVPText(VP_QR_STATUS_TEXT, "Waiting for QR", 32);
  writeVPWord(VP_QR_STATUS_ICON, QR_ICON_WAITING);

  debugLog("startQRScanForBorrower", "Page 4 is now sensing QR automatically.");
}

/*
  Function: processQRData()
  Description:
    Processes scanned QR data and extracts only the borrower name.

    Supported formats:
      Teacher:
        ALJHUN SILVESTRE 9850
        -> code = 9850
        -> name = ALJHUN SILVESTRE

      Student:
        TUPC-23-0464 DEFANTE DHAN BET-ESET 5916
        -> code = TUPC-23-0464
        -> name = DEFANTE DHAN

    Manual name is only required if the QR data cannot produce a name.

  How to use:
    Called automatically by handleQRScanner() after one QR code is received.
*/
void processQRData(String qrData) {
  registerUserActivity("QR scanner");
  qrScanActive = false;

  qrData = normalizeQRData(qrData);

  if (qrData.length() == 0) {
    pendingQRRawData = "";
    pendingStudentCode = "";
    pendingBorrowerName = "";

    qrScanActive = true;
    debugFail("processQRData", "QR data is empty after cleanup.");
    renderQRBorrowerPage();
    return;
  }

  pendingQRRawData = qrData;
  pendingStudentCode = extractBorrowerCodeFromQR(qrData);
  pendingBorrowerName = extractBorrowerNameFromQR(qrData);
  String rosterName = findStudentNameFromCode(pendingStudentCode);
  if (rosterName.length() > 0) {
    pendingBorrowerName = rosterName;
  }

  debugNumber("processQRData", "Clean QR data length", qrData.length());
  debugValue("processQRData", "Borrower name detected", pendingBorrowerName.length() > 0 ? "YES" : "NO");

  if (pendingBorrowerName.length() > 0) {
    writeVPText(VP_QR_STATUS_TEXT, "QR name detected", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_SCANNED);
  } else {
    writeVPText(VP_QR_STATUS_TEXT, "Manual name required", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_MANUAL_REQUIRED);
  }

  qrScanActive = pendingBorrowerName.length() == 0;
  renderQRBorrowerPage();
}

/*
  Function: normalizeQRData()
  Description:
    Cleans QR text by converting separators to spaces and removing extra spaces.

  How to use:
    String cleanData = normalizeQRData(rawQR);
*/
String normalizeQRData(String qrData) {
  qrData.replace('\t', ' ');
  qrData.replace('\n', ' ');
  qrData.replace('\r', ' ');
  qrData.replace(',', ' ');
  qrData.replace('|', ' ');
  qrData.replace(';', ' ');

  qrData.trim();

  while (qrData.indexOf("  ") >= 0) {
    qrData.replace("  ", " ");
  }

  return qrData;
}

/*
  Function: getFirstQRToken()
  Description:
    Returns the first space-separated token from cleaned QR data.

  How to use:
    String first = getFirstQRToken(qrData);
*/
String getFirstQRToken(String qrData) {
  qrData = normalizeQRData(qrData);

  int firstSpace = qrData.indexOf(' ');

  if (firstSpace < 0) return qrData;

  String token = qrData.substring(0, firstSpace);
  token.trim();

  return token;
}

/*
  Function: getLastQRToken()
  Description:
    Returns the last space-separated token from cleaned QR data.

  How to use:
    String last = getLastQRToken(qrData);
*/
String getLastQRToken(String qrData) {
  qrData = normalizeQRData(qrData);

  int lastSpace = qrData.lastIndexOf(' ');

  if (lastSpace < 0) return qrData;

  String token = qrData.substring(lastSpace + 1);
  token.trim();

  return token;
}

/*
  Function: removeFirstQRToken()
  Description:
    Removes the first token from cleaned QR data.

  How to use:
    String rest = removeFirstQRToken(qrData);
*/
String removeFirstQRToken(String qrData) {
  qrData = normalizeQRData(qrData);

  int firstSpace = qrData.indexOf(' ');

  if (firstSpace < 0) return "";

  String rest = qrData.substring(firstSpace + 1);
  rest.trim();

  return rest;
}

/*
  Function: removeLastQRToken()
  Description:
    Removes the last token from cleaned QR data.

  How to use:
    String rest = removeLastQRToken(qrData);
*/
String removeLastQRToken(String qrData) {
  qrData = normalizeQRData(qrData);

  int lastSpace = qrData.lastIndexOf(' ');

  if (lastSpace < 0) return "";

  String rest = qrData.substring(0, lastSpace);
  rest.trim();

  return rest;
}

/*
  Function: isNumericQRToken()
  Description:
    Checks if a token contains digits only.

  How to use:
    if (isNumericQRToken("9850")) { ... }
*/
bool isNumericQRToken(String token) {
  token.trim();

  if (token.length() == 0) return false;

  for (uint16_t i = 0; i < token.length(); i++) {
    if (!isDigit(token.charAt(i))) return false;
  }

  return true;
}

/*
  Function: isStudentFormatQR()
  Description:
    Detects the student QR format by checking if the first token starts with TUPC-.

  How to use:
    if (isStudentFormatQR(qrData)) { ... }
*/
bool isStudentFormatQR(String qrData) {
  String first = getFirstQRToken(qrData);
  first.toUpperCase();

  return first.startsWith("TUPC-");
}

/*
  Function: isCourseOrSectionQRToken()
  Description:
    Detects course/section tokens that should not be included in the borrower name.
    Example:
      BET-ESET

  How to use:
    if (isCourseOrSectionQRToken(token)) stop adding name tokens.
*/
bool isCourseOrSectionQRToken(String token) {
  token.trim();
  token.toUpperCase();

  if (token.length() == 0) return false;

  if (token == "BET-ESET") return true;
  if (token.startsWith("BET-")) return true;
  if (token.startsWith("BS") && token.length() >= 4) return true;

  return false;
}

/*
  Function: extractBorrowerCodeFromQR()
  Description:
    Extracts the borrower code from teacher or student QR data.

    Teacher:
      ALJHUN SILVESTRE 9850
      -> 9850

    Student:
      TUPC-23-0464 DEFANTE DHAN BET-ESET 5916
      -> TUPC-23-0464

  How to use:
    String code = extractBorrowerCodeFromQR(qrData);
*/
String extractBorrowerCodeFromQR(String qrData) {
  qrData = normalizeQRData(qrData);

  if (qrData.length() == 0) return "";

  String first = getFirstQRToken(qrData);
  String last = getLastQRToken(qrData);

  if (isStudentFormatQR(qrData)) {
    return first;
  }

  if (isNumericQRToken(last)) {
    return last;
  }

  return first;
}

/*
  Function: extractBorrowerNameFromQR()
  Description:
    Extracts only the borrower name from teacher or student QR data.

    Teacher:
      ALJHUN SILVESTRE 9850
      -> ALJHUN SILVESTRE

    Student:
      TUPC-23-0464 DEFANTE DHAN BET-ESET 5916
      -> DEFANTE DHAN

  How to use:
    String name = extractBorrowerNameFromQR(qrData);
*/
String extractBorrowerNameFromQR(String qrData) {
  qrData = normalizeQRData(qrData);

  if (qrData.length() == 0) return "";

  if (isStudentFormatQR(qrData)) {
    String rest = removeFirstQRToken(qrData);

    if (isNumericQRToken(getLastQRToken(rest))) {
      rest = removeLastQRToken(rest);
    }

    String name = "";

    while (rest.length() > 0) {
      String token = getFirstQRToken(rest);

      if (isCourseOrSectionQRToken(token)) {
        break;
      }

      if (name.length() > 0) name += " ";
      name += token;

      rest = removeFirstQRToken(rest);
    }

    name.trim();
    return name;
  }

  String last = getLastQRToken(qrData);

  if (isNumericQRToken(last)) {
    String name = removeLastQRToken(qrData);
    name.trim();
    return name;
  }

  String fallbackName = removeFirstQRToken(qrData);
  fallbackName.trim();

  return fallbackName;
}

/*
  Function: findStudentNameFromCode()
  Description:
    Kept for future CSV database use.
    Current QR parser already extracts the name from the QR text itself.

  How to use:
    String name = findStudentNameFromCode("TUPC-23-0464");
*/
String findStudentNameFromCode(String studentCode) {
  studentCode.trim();
  if (studentCode.length() == 0 || !SD.begin(SD_STORAGE_CS, SPI)) {
    return "";
  }

  File roster = SD.open("/students.csv", FILE_READ);
  if (!roster) {
    SD.end();
    return "";
  }

  String matchedName = "";
  while (roster.available()) {
    String line = readCSVRecord(roster);
    if (line.length() == 0) continue;

    String fields[2];
    if (!parseCSVFields(line, fields, 2)) continue;
    String code = fields[0];
    String name = fields[1];
    code.trim();
    name.trim();

    if (code.equalsIgnoreCase(studentCode)) {
      matchedName = name;
      break;
    }
  }

  roster.close();
  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);
  return matchedName;
}

/*
  Function: confirmBorrowerIdentity()
  Description:
    Confirms Page 4 borrower identity after manual name entry or QR parsing.

    Current working methods:
      Manual borrower name -> OK -> log transaction -> mark slot taken -> unlock output.
      QR borrower name     -> OK -> log transaction -> mark slot taken -> unlock output.

  How to use:
    Called when VP_SLOT_CONFIRM 0x1028 is pressed on Page 4.
*/
void confirmBorrowerIdentity() {
  debugLog("confirmBorrowerIdentity", "Confirm borrower button pressed.");

  if (selectedSlotIndex < 0) {
    writeVPText(VP_QR_STATUS_TEXT, "No selected slot", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_ERROR);
    debugFail("confirmBorrowerIdentity", "No selected slot.");
    return;
  }

  if (pendingBorrowerName.length() == 0) {
    debugLog("confirmBorrowerIdentity", "Borrower name is empty; cancelling borrow and clearing the selected slot.");
    cancelQRBorrowerPage();
    return;
  }

  String slotNo = String(selectedSlotIndex + 1);
  String keyName = String(selectedSlotIndex + 1);
  String keyNumber = slotNo;

  KeyEntry selectedKey;

  if (getRegisteredKeyForSlot(selectedSlotIndex, selectedKey)) {
    keyName = selectedKey.keyName;
    keyNumber = selectedKey.keyNumber;
    debugLog("confirmBorrowerIdentity", "Registered key found for selected slot.");
  } else {
    debugNumber("confirmBorrowerIdentity", "No registered key for slot, using fallback slot number", selectedSlotIndex + 1);
  }

  String borrowedTime = getRTCTimeString();
  String today = getRTCDateString();

  String transactionUser = currentHomeUserName.length() > 0 ? currentHomeUserName : "Unknown";
  debugValue("confirmBorrowerIdentity", "TUP code captured",
             pendingStudentCode.length() > 0 ? "YES" : "NO");

  if (!addTransactionLog(keyNumber, pendingBorrowerName, keyName,
                         borrowedTime, "--:--", transactionUser, today,
                         pendingStudentCode)) {
    writeVPText(VP_QR_STATUS_TEXT, "Log save failed", 32);
    writeVPWord(VP_QR_STATUS_ICON, QR_ICON_ERROR);
    debugFail("confirmBorrowerIdentity", "Borrow cancelled because the daily transaction could not be committed.");
    return;
  }

  addCurrentLog(keyName, pendingBorrowerName, keyNumber, borrowedTime, LOG_STATUS_BORROWED);

  slotTaken[selectedSlotIndex] = true;
  saveSlotTakenState((uint8_t)selectedSlotIndex);
  slotDisplayState[selectedSlotIndex] = SLOT_TAKEN;
  slotSelected[selectedSlotIndex] = false;
  updateSlotDisplay(selectedSlotIndex);

  setSlotLockOutput(selectedSlotIndex, true);
  startTimedSlotUnlock((uint8_t)selectedSlotIndex);

  writeVPText(VP_QR_STATUS_TEXT, "Borrow confirmed", 32);
  writeVPWord(VP_QR_STATUS_ICON, QR_ICON_CONFIRMED);

  debugLog("confirmBorrowerIdentity", "Borrow identity confirmed.");
  debugNumber("confirmBorrowerIdentity", "Slot", selectedSlotIndex + 1);

  delayWithUnlockService(500);

  int confirmedSlot = selectedSlotIndex;
  selectedSlotIndex = -1;
  pendingBorrowerName = "";
  pendingStudentCode = "";
  pendingQRRawData = "";
  qrScanActive = false;

  if (qrReturnPage == PAGE_HOME_DASHBOARD_B) {
    goToHomeDashboard(PAGE_HOME_DASHBOARD_B);
  } else {
    goToHomeDashboard(PAGE_HOME_DASHBOARD_A);
  }

  debugNumber("confirmBorrowerIdentity", "Unlocked output for slot", confirmedSlot + 1);
}

/*
  Function: cancelQRBorrowerPage()
  Description:
    Cancels Page 4 borrower identification and returns to the previous
    Home Dashboard page.

  How to use:
    Called when Page 4 Cancel button sends VP 0x1642.
*/
void cancelQRBorrowerPage() {
  qrScanActive = false;
  qrBuffer = "";
  pendingBorrowerName = "";
  pendingStudentCode = "";
  pendingQRRawData = "";

  if (selectedSlotIndex >= 0) {
    slotSelected[selectedSlotIndex] = false;
    updateSlotDisplay(selectedSlotIndex);
  }

  selectedSlotIndex = -1;

  if (qrReturnPage == PAGE_HOME_DASHBOARD_B) {
    goToHomeDashboard(PAGE_HOME_DASHBOARD_B);
  } else {
    goToHomeDashboard(PAGE_HOME_DASHBOARD_A);
  }
}
