// =======================================================
// StorageUSB.ino
// SD modules and CH376S USB download placeholder.
// =======================================================

bool isValidArchiveDate(const String &dateText) {
  if (dateText.length() != 10 || dateText.charAt(4) != '-' || dateText.charAt(7) != '-') return false;
  for (uint8_t i = 0; i < dateText.length(); i++) {
    if (i == 4 || i == 7) continue;
    if (!isDigit(dateText.charAt(i))) return false;
  }
  return true;
}

String csvEscapeField(const String &field) {
  if (field.indexOf(',') < 0 && field.indexOf('"') < 0 &&
      field.indexOf('\n') < 0 && field.indexOf('\r') < 0) {
    return field;
  }

  String escaped = "\"";
  for (uint16_t i = 0; i < field.length(); i++) {
    if (field.charAt(i) == '"') escaped += '"';
    escaped += field.charAt(i);
  }
  escaped += '"';
  return escaped;
}

bool parseCSVFields(String line, String *fields, uint8_t fieldCount) {
  uint8_t fieldIndex = 0;
  bool insideQuotes = false;
  bool closedQuote = false;
  fields[0] = "";

  for (uint16_t i = 0; i < line.length(); i++) {
    char c = line.charAt(i);

    if (insideQuotes) {
      if (c == '"' && i + 1 < line.length() && line.charAt(i + 1) == '"') {
        fields[fieldIndex] += '"';
        i++;
      } else if (c == '"') {
        insideQuotes = false;
        closedQuote = true;
      } else {
        fields[fieldIndex] += c;
      }
    } else if (c == ',') {
      if (++fieldIndex >= fieldCount) return false;
      fields[fieldIndex] = "";
      closedQuote = false;
    } else if (c == '"' && fields[fieldIndex].length() == 0 && !closedQuote) {
      insideQuotes = true;
    } else if (!closedQuote) {
      fields[fieldIndex] += c;
    } else if (c != ' ' && c != '\t') {
      return false;
    }
  }

  return !insideQuotes && fieldIndex == fieldCount - 1;
}

String readCSVRecord(File &file) {
  String record;
  bool insideQuotes = false;

  while (file.available()) {
    updateReturnUnlockRelock();
    char c = (char)file.read();

    if (c == '"') {
      record += c;
      if (insideQuotes && file.peek() == '"') {
        record += (char)file.read();
      } else {
        insideQuotes = !insideQuotes;
      }
    } else if (c == '\n' && !insideQuotes) {
      break;
    } else if (c != '\r' || insideQuotes) {
      record += c;
    }
  }

  return record;
}

bool parseTransactionCSVFields(String line, String *fields) {
  if (parseCSVFields(line, fields, 8)) return true;
  if (!parseCSVFields(line, fields, 7)) return false;
  fields[7] = "";
  return true;
}

String archiveEntryName(File &entry) {
  String name = entry.name();
  name.trim();
  while (name.endsWith("/")) name.remove(name.length() - 1);
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  return name;
}

bool insertArchiveDate(String *dates, uint8_t &count, const String &dateText) {
  if (!isValidArchiveDate(dateText)) return false;

  uint8_t position = 0;
  while (position < count && dates[position] < dateText) position++;
  if (position < count && dates[position] == dateText) return false;
  if (count == MAX_DATE_LOGS && position == count) return false;

  uint8_t newCount = count < MAX_DATE_LOGS ? count + 1 : count;
  for (int i = newCount - 1; i > position; i--) {
    dates[i] = dates[i - 1];
  }
  dates[position] = dateText;
  count = newCount;
  return true;
}

void addLoadedTransaction(const String *fields) {
  if (transactionLogCount >= MAX_TRANSACTION_LOGS) {
    for (uint8_t i = 1; i < MAX_TRANSACTION_LOGS; i++) {
      transactionLogs[i - 1] = transactionLogs[i];
    }
    transactionLogCount = MAX_TRANSACTION_LOGS - 1;
  }

  TransactionLogEntry &entry = transactionLogs[transactionLogCount++];
  entry.dateText = fields[0];
  entry.keyNo = fields[1];
  entry.borrowerName = fields[2];
  entry.keyName = fields[3];
  entry.borrowedTime = fields[4];
  entry.returnedTime = fields[5];
  entry.userName = fields[6];
}

bool archiveContainsLine(const String &path, const String &line) {
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return false;
  bool found = false;
  while (file.available()) {
    String existing = readCSVRecord(file);
    if (existing == line) {
      found = true;
      break;
    }
  }
  file.close();
  return found;
}

bool appendMountedArchiveLine(const String &path, const String &line) {
  File file = SD.open(path.c_str(), FILE_APPEND);
  if (!file) return false;

  uint32_t originalSize = file.size();
  size_t written = file.println(line);
  file.flush();
  file.close();
  if (written != line.length() + 2) return false;

  File verify = SD.open(path.c_str(), FILE_READ);
  if (!verify || verify.size() != originalSize + written ||
      !verify.seek(originalSize)) {
    if (verify) verify.close();
    return false;
  }

  String savedLine = readCSVRecord(verify);
  verify.close();
  if (savedLine.endsWith("\r")) savedLine.remove(savedLine.length() - 1);
  return savedLine == line;
}

void queuePendingMonthlyMirror(const String &dateText,
                               const String &fileName,
                               const String &line) {
  for (uint8_t i = 0; i < pendingMonthlyMirrorCount; i++) {
    if (pendingMonthlyMirrors[i].dateText == dateText &&
        pendingMonthlyMirrors[i].fileName == fileName &&
        pendingMonthlyMirrors[i].line == line) {
      return;
    }
  }

  if (pendingMonthlyMirrorCount >= MAX_PENDING_MONTHLY_MIRRORS) {
    debugFail("queuePendingMonthlyMirror", "Retry queue is full; the verified daily archive remains authoritative and startup reconciliation can retry the mirror.");
    return;
  }

  PendingMonthlyMirror &pending = pendingMonthlyMirrors[pendingMonthlyMirrorCount++];
  pending.dateText = dateText;
  pending.fileName = fileName;
  pending.line = line;
}

void queueMissingMonthlyMirrorsForDate(const String &dateText) {
  const char *archiveNames[] = {
    "accounts.csv", "keys.csv", "transactions.csv", "returns.csv"
  };

  for (uint8_t i = 0; i < 4; i++) {
    String dailyPath = "/daily/" + dateText + "/" + archiveNames[i];
    File dailyFile = SD.open(dailyPath.c_str(), FILE_READ);
    if (!dailyFile) continue;

    String monthlyPath = "/monthly/" + dateText.substring(0, 7) + "/" + archiveNames[i];
    while (dailyFile.available()) {
      String line = readCSVRecord(dailyFile);
      if (line.length() == 0 || archiveContainsLine(monthlyPath, line)) continue;
      queuePendingMonthlyMirror(dateText, archiveNames[i], line);
    }
    dailyFile.close();
  }
}

bool retryPendingMonthlyMirrors() {
  if (pendingMonthlyMirrorCount == 0 ||
      millis() - lastMonthlyMirrorRetry < 30000UL) {
    return true;
  }
  lastMonthlyMirrorRetry = millis();

  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    storageSDReady = false;
    debugFail("retryPendingMonthlyMirrors", "SD card unavailable; monthly mirror retry remains pending.");
    return false;
  }

  uint8_t index = 0;
  while (index < pendingMonthlyMirrorCount) {
    PendingMonthlyMirror &pending = pendingMonthlyMirrors[index];
    String monthlyDirectory = "/monthly/" + pending.dateText.substring(0, 7);
    String monthlyPath = monthlyDirectory + "/" + pending.fileName;
    bool mirrored = ensureArchiveDirectory("/monthly") &&
                    ensureArchiveDirectory(monthlyDirectory);

    if (mirrored && !archiveContainsLine(monthlyPath, pending.line)) {
      mirrored = appendMountedArchiveLine(monthlyPath, pending.line);
    }

    if (!mirrored) {
      debugValue("retryPendingMonthlyMirrors", "Still pending", monthlyPath);
      index++;
      continue;
    }

    for (uint8_t moveIndex = index + 1;
         moveIndex < pendingMonthlyMirrorCount;
         moveIndex++) {
      pendingMonthlyMirrors[moveIndex - 1] = pendingMonthlyMirrors[moveIndex];
    }
    pendingMonthlyMirrorCount--;
  }

  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);
  storageSDReady = true;
  return pendingMonthlyMirrorCount == 0;
}

bool migrateLegacyRootLogs() {
  const char *legacyFiles[] = {
    "/accounts.csv", "/keys.csv", "/transactions.csv", "/returns.csv"
  };
  bool migrationSucceeded = true;

  for (uint8_t i = 0; i < 4; i++) {
    String sourcePath = legacyFiles[i];
    if (!SD.exists(sourcePath.c_str())) continue;

    File source = SD.open(sourcePath.c_str(), FILE_READ);
    if (!source) {
      migrationSucceeded = false;
      continue;
    }

    String fileName = sourcePath.substring(sourcePath.lastIndexOf('/') + 1);
    while (source.available()) {
      String line = readCSVRecord(source);
      if (line.length() == 0) continue;

      String dateText = line.substring(0, 10);
      if (!isValidArchiveDate(dateText)) {
        debugFail("migrateLegacyRootLogs", "Skipped CSV row with invalid date.");
        migrationSucceeded = false;
        continue;
      }

      String dailyDirectory = "/daily/" + dateText;
      String monthlyDirectory = "/monthly/" + dateText.substring(0, 7);
      if (!ensureArchiveDirectory("/daily") ||
          !ensureArchiveDirectory("/monthly") ||
          !ensureArchiveDirectory(dailyDirectory) ||
          !ensureArchiveDirectory(monthlyDirectory)) {
        migrationSucceeded = false;
        continue;
      }

      String dailyPath = dailyDirectory + "/" + fileName;
      String monthlyPath = monthlyDirectory + "/" + fileName;
      if (!archiveContainsLine(dailyPath, line) &&
          !appendMountedArchiveLine(dailyPath, line)) {
        migrationSucceeded = false;
        continue;
      }
      if (!archiveContainsLine(monthlyPath, line) &&
          !appendMountedArchiveLine(monthlyPath, line)) {
        migrationSucceeded = false;
      }
    }
    source.close();

    if (migrationSucceeded && !SD.remove(sourcePath.c_str())) {
      migrationSucceeded = false;
    }
  }

  return migrationSucceeded;
}

bool loadLogsFromSD() {
  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    storageSDReady = false;
    debugFail("loadLogsFromSD", "SD card not available at CS GPIO14.");
    return false;
  }

  if (!migrateLegacyRootLogs()) {
    debugFail("loadLogsFromSD", "Some legacy root CSV records could not be archived.");
  }

  String dates[MAX_DATE_LOGS];
  uint8_t loadedDateCount = 0;
  File dailyRoot = SD.open("/daily");
  if (!dailyRoot || !dailyRoot.isDirectory()) {
    if (dailyRoot) dailyRoot.close();
    SD.end();
    debugLog("loadLogsFromSD", "No daily archive directory exists yet.");
    storageSDReady = true;
    return true;
  }

  File entry = dailyRoot.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      insertArchiveDate(dates, loadedDateCount, archiveEntryName(entry));
    }
    entry.close();
    entry = dailyRoot.openNextFile();
  }
  dailyRoot.close();

  transactionLogCount = 0;
  transactionLogOffset = 0;
  dateLogCount = 0;
  dateCardOffset = 0;
  selectedDateVisibleIndex = -1;
  selectedDateText = "";

  for (uint8_t i = 0; i < loadedDateCount; i++) {
    addDateLogIfMissing(dates[i]);
    queueMissingMonthlyMirrorsForDate(dates[i]);
    String transactionPath = "/daily/" + dates[i] + "/transactions.csv";
    File transactionFile = SD.open(transactionPath.c_str(), FILE_READ);
    if (!transactionFile) continue;

    while (transactionFile.available()) {
      String line = readCSVRecord(transactionFile);
      if (line.length() == 0) continue;

      String fields[8];
      if (parseTransactionCSVFields(line, fields) && fields[0] == dates[i]) {
        addLoadedTransaction(fields);
      } else {
        debugFail("loadLogsFromSD", "Skipped malformed transaction row.");
      }
    }
    transactionFile.close();
  }

  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);
  storageSDReady = true;

  if (dateLogCount > 0) {
    selectedDateText = dateLogs[dateLogCount - 1];
    dateCardOffset = dateLogCount > DATE_CARD_COUNT ? dateLogCount - DATE_CARD_COUNT : 0;
    selectedDateVisibleIndex = dateLogCount - 1 - dateCardOffset;
  }
  transactionLogOffset = 0;

  String today = getRTCDateString();
  if (today != "0000-00-00") {
    loadCurrentLogsForDateFromSD(today);
  } else {
    debugFail("loadLogsFromSD", "RTC date is invalid; current-day logs were not restored.");
  }

  debugNumber("loadLogsFromSD", "Daily dates loaded", dateLogCount);
  debugNumber("loadLogsFromSD", "Transaction rows loaded", transactionLogCount);
  return true;
}

bool ensureArchiveDirectory(const String &path) {
  if (SD.exists(path.c_str())) return true;
  return SD.mkdir(path.c_str());
}

bool removeArchiveDirectoryTree(const String &path) {
  File directory = SD.open(path.c_str());
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return false;
  }

  bool removedAll = true;
  File child = directory.openNextFile();
  while (child) {
    String childPath = path + "/" + archiveEntryName(child);
    bool isDirectory = child.isDirectory();
    child.close();

    bool removed = isDirectory
        ? (removeArchiveDirectoryTree(childPath) && SD.rmdir(childPath.c_str()))
        : SD.remove(childPath.c_str());
    if (!removed) removedAll = false;
    child = directory.openNextFile();
  }
  directory.close();
  return removedAll;
}

/*
  Daily event files are archived under /daily/YYYY-MM-DD/ and mirrored
  under /monthly/YYYY-MM/. Both folders live on the same GPIO14 SD card.
*/
bool initStorageModules() {
  debugLog("initStorageModules", "Starting single SD card checks on GPIO14.");

  digitalWrite(RFID_CS, HIGH);
  digitalWrite(USB_CS, HIGH);
  digitalWrite(SD_STORAGE_CS, HIGH);

  storageSDReady = SD.begin(SD_STORAGE_CS, SPI);
  if (!storageSDReady) {
    debugFail("initStorageModules", "SD.begin failed at CS GPIO14.");
    return false;
  }

  bool directoriesReady = ensureArchiveDirectory("/daily") &&
                          ensureArchiveDirectory("/monthly");
  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);

  storageSDReady = directoriesReady;
  if (!storageSDReady) {
    debugFail("initStorageModules", "Could not create daily/monthly archive directories.");
    return false;
  }

  debugOK("initStorageModules");
  return true;
}

/*
  Performs the CH376 CHECK_EXIST command before calling library initialization.
  This prevents the library's startup interrupt-drain loop from running when
  the controller is not present.
*/
bool probeCH376Controller() {
  digitalWrite(SD_STORAGE_CS, HIGH);
  digitalWrite(RFID_CS, HIGH);
  digitalWrite(USB_CS, HIGH);

  SPI.beginTransaction(SPISettings(125000, MSBFIRST, SPI_MODE0));
  digitalWrite(USB_CS, LOW);
  SPI.transfer(0x06);
  SPI.transfer(0x01);
  uint8_t response = SPI.transfer(0x00);
  digitalWrite(USB_CS, HIGH);
  SPI.endTransaction();

  debugHex16("probeCH376Controller", "CHECK_EXIST response", response);
  return response == 0xFE;
}

/*
  Function: initUSBHost()
  Description:
    Initializes and verifies the CH376S SPI host controller.

  How to use:
    Called during loading screen.
*/
bool initUSBHost() {
  digitalWrite(USB_CS, HIGH);
  digitalWrite(SD_STORAGE_CS, HIGH);
  digitalWrite(RFID_CS, HIGH);

  if (!probeCH376Controller()) {
    usbReady = false;
    usbInserted = false;
    previousUSBInserted = false;
    debugFail("initUSBHost", "CH376S controller was not detected by SPI CHECK_EXIST; skipping library initialization.");
    return false;
  }

  flashDrive.init();
  usbReady = flashDrive.getCHpresence();
  if (!usbReady) {
    usbInserted = false;
    previousUSBInserted = false;
    debugFail("initUSBHost", "CH376S did not respond over SPI; check CS GPIO13, INT GPIO25, shared SPI, power, and SPI mode.");
    return false;
  }

  usbInserted = flashDrive.getDeviceStatus();
  previousUSBInserted = usbInserted;
  debugOK("initUSBHost");
  debugValue("initUSBHost", "USB drive mounted", usbInserted ? "YES" : "NO");
  return true;
}

/*
  Function: usbHostConnected()
  Description:
    Checks whether a USB drive is inserted in the CH376S USB host.

  How to use:
    bool inserted = usbHostConnected();

  The CH376 library's interrupt poll updates its mounted-drive status.
*/
bool usbHostConnected() {
  return usbReady && flashDrive.getDeviceStatus();
}

void showDownloadStatus(uint16_t status) {
  writeVPWord(VP_DOWNLOAD_ICON, status);

  if (status == DOWNLOAD_USB_CONNECTED || status == DOWNLOAD_SUCCESS ||
      status == DOWNLOAD_USB_NOT_FOUND || status == DOWNLOAD_FAILED) {
    downloadStatusClearAt = millis() + DOWNLOAD_STATUS_DISPLAY_MS;
    downloadStatusClearPending = true;
  } else {
    downloadStatusClearPending = false;
  }
}

void updateDownloadStatus() {
  if (!downloadStatusClearPending ||
      static_cast<int32_t>(millis() - downloadStatusClearAt) < 0) {
    return;
  }

  downloadStatusClearPending = false;
  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_IDLE);
}

/*
  Function: monitorUSBInsertion()
  Description:
    Checks the USB host repeatedly in the background.
    This gives a laptop-like behavior: when a USB is inserted or removed,
    the display can update its download icon even before Download is pressed.

  How to use:
    Called automatically in loop().
*/
void monitorUSBInsertion() {
  if (!usbReady) return;

  bool interruptReported = flashDrive.checkIntMessage();
  if (!interruptReported && millis() - lastUSBMonitorCheck < 1000) return;
  lastUSBMonitorCheck = millis();

  usbInserted = usbHostConnected();

  if (usbInserted != previousUSBInserted) {
    if (usbInserted) {
      notifyUSBInserted();
    } else {
      notifyUSBRemoved();
    }

    previousUSBInserted = usbInserted;
  }
}

bool writeUSBBytes(const uint8_t *data, size_t length,
                   uint32_t &bytesWritten) {
  uint8_t buffer[64];
  size_t offset = 0;

  while (offset < length) {
    updateReturnUnlockRelock();
    size_t chunkSize = length - offset;
    if (chunkSize > sizeof(buffer)) chunkSize = sizeof(buffer);
    memcpy(buffer, data + offset, chunkSize);
    if (!flashDrive.writeFile(reinterpret_cast<char *>(buffer),
                              static_cast<uint8_t>(chunkSize))) {
      return false;
    }
    offset += chunkSize;
    bytesWritten += static_cast<uint32_t>(chunkSize);
  }

  return true;
}

bool writeUSBTransactionCSV(File &source, uint32_t &bytesWritten) {
  const String header =
      "DATE,Borrower NAME,KEY NAME,ACTION,TIME,TUP CODE,USER Registered\r\n";
  if (!writeUSBBytes(reinterpret_cast<const uint8_t *>(header.c_str()),
                     header.length(), bytesWritten)) {
    return false;
  }

  while (source.available()) {
    String line = readCSVRecord(source);
    if (line.length() == 0) continue;

    String fields[8];
    if (!parseTransactionCSVFields(line, fields) ||
        !isValidArchiveDate(fields[0])) {
      debugFail("writeUSBTransactionCSV", "Malformed transaction row; export stopped.");
      return false;
    }

    bool hasBorrowTime = fields[4].length() > 0 && fields[4] != "--:--";
    bool hasReturnTime = fields[5].length() > 0 && fields[5] != "--:--";
    if (!hasBorrowTime && !hasReturnTime) {
      debugFail("writeUSBTransactionCSV", "Transaction row has no borrow or return time; export stopped.");
      return false;
    }

    for (uint8_t actionIndex = 0; actionIndex < 2; actionIndex++) {
      bool isBorrow = actionIndex == 0;
      if ((isBorrow && !hasBorrowTime) || (!isBorrow && !hasReturnTime)) continue;

      String action = isBorrow ? "Borrow" : "Return";
      String time = isBorrow ? fields[4] : fields[5];
      String record = csvEscapeField(fields[0]) + "," +
                      csvEscapeField(fields[2]) + "," +
                      csvEscapeField(fields[3]) + "," +
                      csvEscapeField(action) + "," +
                      csvEscapeField(time) + "," +
                      csvEscapeField(fields[7]) + "," +
                      csvEscapeField(fields[6]) + "\r\n";
      if (!writeUSBBytes(reinterpret_cast<const uint8_t *>(record.c_str()),
                         record.length(), bytesWritten)) {
        return false;
      }
    }
  }

  return true;
}

bool verifyUSBTransactionCSVHeader(const char *fileName) {
  const char *expectedHeader =
      "DATE,Borrower NAME,KEY NAME,ACTION,TIME,TUP CODE,USER Registered\r\n";
  const size_t expectedHeaderLength = strlen(expectedHeader);

  flashDrive.setFileName(fileName);
  if (flashDrive.openFile() != ANSW_USB_INT_SUCCESS) {
    debugValue("verifyUSBTransactionCSVHeader", "Could not reopen transaction export", fileName);
    return false;
  }

  char headerBuffer[96] = {};
  flashDrive.readFile(headerBuffer, sizeof(headerBuffer));
  bool headerMatches =
      flashDrive.getStreamLen() >= expectedHeaderLength &&
      memcmp(headerBuffer, expectedHeader, expectedHeaderLength) == 0;
  uint8_t closeStatus = flashDrive.closeFile();

  if (!headerMatches) {
    debugFail("verifyUSBTransactionCSVHeader", "Exported transaction file does not begin with the required header.");
  }
  if (closeStatus != ANSW_USB_INT_SUCCESS) {
    debugHex16("verifyUSBTransactionCSVHeader", "USB file close status", closeStatus);
    return false;
  }

  return headerMatches;
}

bool copyDailyArchiveFileToUSB(const String &dateText,
                               const char *archiveFileName,
                               char categoryCode,
                               bool &sourceExists,
                               bool &sourceHasData) {
  String sourcePath = "/daily/" + dateText + "/" + archiveFileName;
  sourceExists = SD.exists(sourcePath.c_str());
  sourceHasData = false;
  if (!sourceExists) return true;

  File source = SD.open(sourcePath.c_str(), FILE_READ);
  if (!source) {
    debugValue("copyDailyArchiveFileToUSB", "Could not open SD archive", sourcePath);
    return false;
  }

  uint32_t expectedSize = source.size();
  sourceHasData = expectedSize > 0;
  if (!sourceHasData) {
    source.close();
    debugValue("copyDailyArchiveFileToUSB", "Skipped empty archive", sourcePath);
    return true;
  }

  String shortDate = dateText.substring(2, 4) +
                     dateText.substring(5, 7) +
                     dateText.substring(8, 10);
  char destinationName[13];
  snprintf(destinationName, sizeof(destinationName), "%s%c.CSV",
           shortDate.c_str(), categoryCode);

  flashDrive.setFileName(destinationName);
  uint8_t existingStatus = flashDrive.openFile();
  if (existingStatus == ANSW_USB_INT_SUCCESS) {
    uint8_t existingCloseStatus = flashDrive.closeFile();
    if (existingCloseStatus != ANSW_USB_INT_SUCCESS) {
      source.close();
      debugValue("copyDailyArchiveFileToUSB", "Could not close existing USB file", destinationName);
      debugHex16("copyDailyArchiveFileToUSB", "Existing file close status", existingCloseStatus);
      return false;
    }

    flashDrive.setFileName(destinationName);
    uint8_t deleteStatus = flashDrive.deleteFile();
    if (deleteStatus != ANSW_USB_INT_SUCCESS) {
      source.close();
      debugValue("copyDailyArchiveFileToUSB", "Could not replace USB file", destinationName);
      debugHex16("copyDailyArchiveFileToUSB", "USB delete status", deleteStatus);
      return false;
    }
  } else if (existingStatus != ANSW_ERR_MISS_FILE) {
    source.close();
    debugValue("copyDailyArchiveFileToUSB", "Could not inspect USB destination", destinationName);
    debugHex16("copyDailyArchiveFileToUSB", "USB destination open status", existingStatus);
    return false;
  }

  flashDrive.setFileName(destinationName);
  uint8_t openStatus = flashDrive.openFile();
  if (openStatus != ANSW_ERR_MISS_FILE) {
    source.close();
    debugNumber("copyDailyArchiveFileToUSB", "USB file was not ready for creation; status", openStatus);
    return false;
  }

  uint8_t buffer[64];
  bool writeSucceeded = true;
  uint32_t bytesWritten = 0;
  if (categoryCode == 'T') {
    writeSucceeded = writeUSBTransactionCSV(source, bytesWritten);
  } else {
    while (source.available()) {
      updateReturnUnlockRelock();
      int bytesRead = source.read(buffer, sizeof(buffer));
      if (bytesRead <= 0 ||
          !flashDrive.writeFile(reinterpret_cast<char *>(buffer),
                                static_cast<uint8_t>(bytesRead))) {
        writeSucceeded = false;
        break;
      }
      bytesWritten += static_cast<uint32_t>(bytesRead);
    }
  }
  source.close();

  uint8_t closeStatus = flashDrive.closeFile();
  if (!writeSucceeded || bytesWritten == 0 ||
      (categoryCode != 'T' && bytesWritten != expectedSize) ||
      closeStatus != ANSW_USB_INT_SUCCESS) {
    debugNumber("copyDailyArchiveFileToUSB", "USB write/close status", closeStatus);
    return false;
  }

  flashDrive.setFileName(destinationName);
  uint8_t verifyStatus = flashDrive.openFile();
  if (verifyStatus != ANSW_USB_INT_SUCCESS) {
    debugNumber("copyDailyArchiveFileToUSB", "Could not reopen exported file; status", verifyStatus);
    return false;
  }

  uint32_t destinationSize = flashDrive.getFileSize();
  uint8_t verifyCloseStatus = flashDrive.closeFile();
  if (destinationSize != bytesWritten ||
      verifyCloseStatus != ANSW_USB_INT_SUCCESS) {
    debugNumber("copyDailyArchiveFileToUSB", "Exported file size mismatch", destinationSize);
    return false;
  }

  if (categoryCode == 'T' &&
      !verifyUSBTransactionCSVHeader(destinationName)) {
    return false;
  }

  debugValue("copyDailyArchiveFileToUSB", "Exported USB file", destinationName);
  return true;
}

/*
  Function: notifyUSBInserted()
  Description:
    Updates the USB/download status when a USB flash drive is detected.

  How to use:
    Called automatically by monitorUSBInsertion().
*/
void notifyUSBInserted() {
  debugOK("notifyUSBInserted");
  showDownloadStatus(DOWNLOAD_USB_CONNECTED);
}

/*
  Function: notifyUSBRemoved()
  Description:
    Updates the USB/download status when the USB flash drive is removed.

  How to use:
    Called automatically by monitorUSBInsertion().
*/
void notifyUSBRemoved() {
  debugLog("notifyUSBRemoved", "USB removed or not detected.");

  if (currentPage == PAGE_ADMIN_DASHBOARD) {
    showDownloadStatus(DOWNLOAD_USB_NOT_FOUND);
  } else {
    showDownloadStatus(DOWNLOAD_IDLE);
  }
}

/*
  Function: writeTextFileToDaySD()
  Description:
    Writes a full text file to the primary daily SD card.

  How to use:
    writeTextFileToDaySD("/test.txt", "hello");
*/
bool writeTextFileToDaySD(String path, String content) {
  debugValue("writeTextFileToDaySD", "Path", path);

  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    debugFail("writeTextFileToDaySD", "SD card not available at CS GPIO14.");
    return false;
  }

  File file = SD.open(path, FILE_WRITE);

  if (!file) {
    debugFail("writeTextFileToDaySD", "Failed to open file for writing.");
    SD.end();
    return false;
  }

  size_t written = file.print(content);
  file.close();
  SD.end();

  debugNumber("writeTextFileToDaySD", "Bytes written", written);
  debugOK("writeTextFileToDaySD");

  return true;
}

/*
  Function: appendLineToDaySD()
  Description:
    Appends one line to a file on the primary daily SD card.

  How to use:
    appendLineToDaySD("/keys.csv", "date,key,rfid");
*/
bool appendLineToDaySD(String path, String line) {
  debugValue("appendLineToDaySD", "Path", path);
  debugNumber("appendLineToDaySD", "CSV row length", line.length());

  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    storageSDReady = false;
    debugFail("appendLineToDaySD", "SD card not available at CS GPIO14.");
    return false;
  }

  if (line.length() < 10) {
    SD.end();
    debugFail("appendLineToDaySD", "Log row does not begin with an ISO date.");
    return false;
  }

  String dateText = line.substring(0, 10);
  if (!isValidArchiveDate(dateText)) {
    SD.end();
    debugFail("appendLineToDaySD", "Log row has an invalid ISO date.");
    return false;
  }

  int slash = path.lastIndexOf('/');
  String fileName = slash >= 0 ? path.substring(slash + 1) : path;
  if (fileName != "accounts.csv" && fileName != "keys.csv" &&
      fileName != "transactions.csv" && fileName != "returns.csv") {
    SD.end();
    debugFail("appendLineToDaySD", "Unsupported archive file name.");
    return false;
  }

  String dailyDirectory = "/daily/" + dateText;
  String monthlyDirectory = "/monthly/" + dateText.substring(0, 7);
  if (!ensureArchiveDirectory("/daily") ||
      !ensureArchiveDirectory(dailyDirectory)) {
    SD.end();
    debugFail("appendLineToDaySD", "Could not create daily archive directory.");
    return false;
  }

  String dailyPath = dailyDirectory + "/" + fileName;
  if (!appendMountedArchiveLine(dailyPath, line)) {
    SD.end();
    digitalWrite(SD_STORAGE_CS, HIGH);
    storageSDReady = false;
    debugFail("appendLineToDaySD", "Daily archive row could not be written and verified.");
    return false;
  }

  storageSDReady = true;
  String monthlyPath = monthlyDirectory + "/" + fileName;
  bool monthlyReady = ensureArchiveDirectory("/monthly") &&
                      ensureArchiveDirectory(monthlyDirectory);
  if (monthlyReady && !archiveContainsLine(monthlyPath, line)) {
    monthlyReady = appendMountedArchiveLine(monthlyPath, line);
  }

  if (!monthlyReady) {
    queuePendingMonthlyMirror(dateText, fileName, line);
    debugFail("appendLineToDaySD", "Daily row was verified; monthly mirror failed and was queued for retry.");
  }

  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);

  debugOK("appendLineToDaySD");

  return true;
}

/*
  Exports the selected date's daily CSV archives to the USB drive root.
  Names follow the FAT 8.3 format: YYMMDD plus A/K/T/R and ".CSV".
*/
void downloadSelectedDateLogToUSB() {
  debugValue("downloadSelectedDateLogToUSB", "Requested date", selectedDateText);

  if (!isValidArchiveDate(selectedDateText)) {
    showDownloadStatus(DOWNLOAD_FAILED);
    debugFail("downloadSelectedDateLogToUSB", "Selected date is missing or invalid.");
    return;
  }

  bool deviceAttached = usbHostConnected();
  bool driveReady = deviceAttached && flashDrive.driveReady();
  debugValue("downloadSelectedDateLogToUSB", "USB device attached", deviceAttached ? "YES" : "NO");
  debugValue("downloadSelectedDateLogToUSB", "USB drive mounted/ready", driveReady ? "YES" : "NO");
  if (!driveReady) {
    usbInserted = false;
    previousUSBInserted = false;
    debugHex16("downloadSelectedDateLogToUSB", "CH376 status", flashDrive.getStatus());
    debugHex16("downloadSelectedDateLogToUSB", "CH376 error", flashDrive.getError());
    showDownloadStatus(DOWNLOAD_USB_NOT_FOUND);
    debugFail("downloadSelectedDateLogToUSB", "USB drive is not mounted or ready.");
    return;
  }

  showDownloadStatus(DOWNLOAD_IN_PROGRESS);

  digitalWrite(USB_CS, HIGH);
  digitalWrite(RFID_CS, HIGH);
  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    digitalWrite(SD_STORAGE_CS, HIGH);
    showDownloadStatus(DOWNLOAD_FAILED);
    debugFail("downloadSelectedDateLogToUSB", "SD card not available at CS GPIO14.");
    return;
  }

  const char *archiveNames[] = {
    "accounts.csv", "keys.csv", "transactions.csv", "returns.csv"
  };
  const char categoryCodes[] = { 'A', 'K', 'T', 'R' };
  uint8_t exportedCount = 0;
  bool exportSucceeded = true;
  bool anySourceExists = false;

  for (uint8_t i = 0; i < 4 && exportSucceeded; i++) {
    bool sourceExists = false;
    bool sourceHasData = false;
    exportSucceeded = copyDailyArchiveFileToUSB(
        selectedDateText, archiveNames[i], categoryCodes[i],
        sourceExists, sourceHasData);
    anySourceExists = anySourceExists || sourceExists;
    if (sourceExists && sourceHasData && exportSucceeded) exportedCount++;
  }

  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);

  if (!exportSucceeded || !anySourceExists || exportedCount == 0) {
    showDownloadStatus(DOWNLOAD_FAILED);
    if (!anySourceExists) {
      debugFail("downloadSelectedDateLogToUSB", "No daily archive files exist for the selected date.");
    } else if (exportSucceeded) {
      debugFail("downloadSelectedDateLogToUSB", "Selected date contains no non-empty archive files.");
    } else {
      debugFail("downloadSelectedDateLogToUSB", "One or more daily archive files could not be exported.");
    }
    return;
  }

  showDownloadStatus(DOWNLOAD_SUCCESS);
  debugNumber("downloadSelectedDateLogToUSB", "Archive files exported", exportedCount);
  debugOK("downloadSelectedDateLogToUSB");
}
