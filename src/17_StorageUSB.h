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
    String existing = file.readStringUntil('\n');
    existing.trim();
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
  size_t written = file.println(line);
  file.close();
  return written > 0;
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
      String line = source.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      String dateText = line.substring(0, 10);
      if (!isValidArchiveDate(dateText)) {
        debugValue("migrateLegacyRootLogs", "Skipped row with invalid date", line);
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
    String transactionPath = "/daily/" + dates[i] + "/transactions.csv";
    File transactionFile = SD.open(transactionPath.c_str(), FILE_READ);
    if (!transactionFile) continue;

    while (transactionFile.available()) {
      String line = transactionFile.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      String fields[7];
      if (parseCSVFields(line, fields, 7) && fields[0] == dates[i]) {
        addLoadedTransaction(fields);
      } else {
        debugValue("loadLogsFromSD", "Skipped malformed transaction row", line);
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
  if (transactionLogCount > TRANS_VISIBLE_ROWS) {
    transactionLogOffset = transactionLogCount - TRANS_VISIBLE_ROWS;
  }

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
  Function: initUSBHost()
  Description:
    Initializes the CH376S USB host SPI chip-select line.
    Low-level CH376S SPI commands are left as a replaceable section.

  How to use:
    Called during loading screen.
*/
bool initUSBHost() {
  digitalWrite(USB_CS, HIGH);
  usbReady = true;

  debugLog("initUSBHost", "CH376S USB CS pin ready. Low-level CH376S file copy is still a placeholder.");
  return true;
}

/*
  Function: usbHostConnected()
  Description:
    Checks whether a USB drive is inserted in the CH376S USB host.

  How to use:
    bool inserted = usbHostConnected();

  Important:
    This is still the replaceable CH376S section.
    Add your CH376S library's disk-detect command here.

    The reason this function does not print FAILED every time is because
    monitorUSBInsertion() calls it repeatedly, like a laptop watching for USB insert.
*/
bool usbHostConnected() {
  // TODO:
  // Replace this false return with your CH376S disk-connect check.
  //
  // Example idea with a CH376S library:
  //   return ch376.diskConnected();
  //
  // Until the real CH376S check is added, USB will always appear missing.
  return false;
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
  if (millis() - lastUSBMonitorCheck < 1000) return;

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

/*
  Function: notifyUSBInserted()
  Description:
    Updates the USB/download status when a USB flash drive is detected.

  How to use:
    Called automatically by monitorUSBInsertion().
*/
void notifyUSBInserted() {
  debugOK("notifyUSBInserted");
  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_CONNECTED);
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
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_NOT_FOUND);
  } else {
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_IDLE);
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
  debugValue("appendLineToDaySD", "Line", line);

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
  bool directoriesReady = ensureArchiveDirectory("/daily") &&
                          ensureArchiveDirectory("/monthly") &&
                          ensureArchiveDirectory(dailyDirectory) &&
                          ensureArchiveDirectory(monthlyDirectory);
  if (!directoriesReady) {
    SD.end();
    debugFail("appendLineToDaySD", "Could not create archive directory.");
    return false;
  }

  String dailyPath = dailyDirectory + "/" + fileName;
  String monthlyPath = monthlyDirectory + "/" + fileName;
  File dailyFile = SD.open(dailyPath.c_str(), FILE_APPEND);
  if (!dailyFile) {
    SD.end();
    debugFail("appendLineToDaySD", "Could not open daily archive file.");
    return false;
  }
  size_t dailyWritten = dailyFile.println(line);
  dailyFile.close();

  File monthlyFile = SD.open(monthlyPath.c_str(), FILE_APPEND);
  if (!monthlyFile) {
    SD.end();
    debugFail("appendLineToDaySD", "Daily row saved, but monthly archive file could not be opened.");
    return false;
  }
  size_t monthlyWritten = monthlyFile.println(line);
  monthlyFile.close();
  SD.end();

  if (dailyWritten == 0 || monthlyWritten == 0) {
    debugFail("appendLineToDaySD", "Daily or monthly archive write failed.");
    return false;
  }

  storageSDReady = true;
  debugOK("appendLineToDaySD");

  return true;
}

/*
  Function: downloadSelectedDateLogToUSB()
  Description:
    Copies the selected date log to the inserted USB flash drive.

    Current behavior:
      - Re-checks USB before downloading.
      - Shows Downloading status.
      - Confirms SD availability.
      - Leaves the final CH376S file-copy part as the replaceable section.

  How to use:
    Called by Admin Download button after a date is selected.
*/
void downloadSelectedDateLogToUSB() {
  debugValue("downloadSelectedDateLogToUSB", "Requested date", selectedDateText);

  if (selectedDateText.length() == 0) {
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_FAILED);
    debugFail("downloadSelectedDateLogToUSB", "No selected date.");
    return;
  }

  if (!usbHostConnected()) {
    usbInserted = false;
    previousUSBInserted = false;
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_USB_NOT_FOUND);
    debugFail("downloadSelectedDateLogToUSB", "USB was removed or not detected.");
    return;
  }

  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_IN_PROGRESS);

  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_FAILED);
    debugFail("downloadSelectedDateLogToUSB", "SD card not available at CS GPIO14.");
    return;
  }

  /*
    TODO:
      Add the real CH376S copy command here.

      Intended final flow:
        1. Build source filename from selectedDateText.
        2. Open the selected log file from SD.
        3. Create a new file on the USB flash drive.
        4. Read bytes from SD.
        5. Write bytes to USB using CH376S.
        6. Close both files.
        7. Show DOWNLOAD_SUCCESS.

      For now, this section is intentionally marked failed because
      CH376S low-level file writing has not been connected to a library yet.
  */

  SD.end();

  delay(500);
  writeVPWord(VP_DOWNLOAD_ICON, DOWNLOAD_FAILED);
  debugFail("downloadSelectedDateLogToUSB", "USB detected, but CH376S file-copy code is still placeholder.");
}
