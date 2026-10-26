// =======================================================
// CurrentLogs.ino
// Locked Screen and Home Dashboard current log cards.
// =======================================================

/*
  Function: initCurrentLogs()
  Description:
    Clears the current-log RAM list during startup. The SD archive loader
    restores the current RTC date after loading historical transactions.

  How to use:
    Called during loading screen.
*/
void initCurrentLogs() {
  currentLogCount = 0;
  currentLogOffset = 0;
  currentLogDate = "";
  lastCurrentLogDateSyncCheck = 0;
}

/*
  Function: addCurrentLog()
  Description:
    Adds one current log entry to RAM. Rendering is newest-first, so a new
    event resets the view to the first card.

  How to use:
    Call this after a real borrow/return action.
*/
void addCurrentLog(String keyName, String borrowerName, String keyNumber, String timeText, uint16_t statusIcon) {
  syncCurrentLogsWithRTCDate();

  if (currentLogCount >= MAX_CURRENT_LOGS) {
    for (uint8_t i = 1; i < MAX_CURRENT_LOGS; i++) {
      currentLogs[i - 1] = currentLogs[i];
    }
    currentLogCount = MAX_CURRENT_LOGS - 1;
  }

  currentLogs[currentLogCount].keyName = keyName;
  currentLogs[currentLogCount].borrowerName = borrowerName;
  currentLogs[currentLogCount].keyNumber = keyNumber;
  currentLogs[currentLogCount].timeText = timeText;
  currentLogs[currentLogCount].statusIcon = statusIcon;
  currentLogCount++;
  currentLogOffset = 0;

  if (currentPage == PAGE_LOCKED_SCREEN) {
    renderCurrentLogs(CURRENT_LOG_CARDS_6);
  }
  else if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
    renderCurrentLogs(CURRENT_LOG_CARDS_4);
  }
}

bool loadCurrentLogsForDateFromSD(const String &dateText) {
  if (dateText.length() != 10 || dateText == "0000-00-00") return false;
  if (!SD.begin(SD_STORAGE_CS, SPI)) {
    storageSDReady = false;
    debugFail("loadCurrentLogsForDateFromSD", "SD card is unavailable; keeping current in-memory logs.");
    return false;
  }

  String transactionPath = "/daily/" + dateText + "/transactions.csv";
  File transactionFile;
  if (SD.exists(transactionPath.c_str())) {
    transactionFile = SD.open(transactionPath.c_str(), FILE_READ);
    if (!transactionFile) {
      SD.end();
      digitalWrite(SD_STORAGE_CS, HIGH);
      debugFail("loadCurrentLogsForDateFromSD", "Could not open today's transaction archive.");
      return false;
    }
  }

  currentLogCount = 0;
  currentLogOffset = 0;

  while (transactionFile && transactionFile.available()) {
    String line = transactionFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    String fields[7];
    if (!parseCSVFields(line, fields, 7) || fields[0] != dateText) {
      debugValue("loadCurrentLogsForDateFromSD", "Skipped malformed transaction row", line);
      continue;
    }

    bool isReturn = fields[5] != "--:--";
    String timeText = isReturn ? fields[5] : fields[4];
    uint16_t statusIcon = isReturn ? LOG_STATUS_RETURNED : LOG_STATUS_BORROWED;

    if (currentLogCount >= MAX_CURRENT_LOGS) {
      for (uint8_t i = 1; i < MAX_CURRENT_LOGS; i++) {
        currentLogs[i - 1] = currentLogs[i];
      }
      currentLogCount = MAX_CURRENT_LOGS - 1;
    }

    CurrentLogEntry &entry = currentLogs[currentLogCount++];
    entry.keyName = fields[3];
    entry.borrowerName = isReturn ? "Returned" : fields[2];
    entry.keyNumber = fields[1];
    entry.timeText = timeText;
    entry.statusIcon = statusIcon;
  }

  if (transactionFile) transactionFile.close();
  SD.end();
  digitalWrite(SD_STORAGE_CS, HIGH);
  storageSDReady = true;

  currentLogDate = dateText;
  if (currentPage == PAGE_LOCKED_SCREEN) {
    renderCurrentLogs(CURRENT_LOG_CARDS_6);
  } else if (currentPage == PAGE_HOME_DASHBOARD_A ||
             currentPage == PAGE_HOME_DASHBOARD_B) {
    renderCurrentLogs(CURRENT_LOG_CARDS_4);
  }

  debugValue("loadCurrentLogsForDateFromSD", "Restored date", dateText);
  debugNumber("loadCurrentLogsForDateFromSD", "Current-date rows loaded", currentLogCount);
  return true;
}

void syncCurrentLogsWithRTCDate() {
  if (millis() - lastCurrentLogDateSyncCheck < 1000) return;
  lastCurrentLogDateSyncCheck = millis();

  String today = getRTCDateString();
  if (today == "0000-00-00" || today == currentLogDate) return;

  loadCurrentLogsForDateFromSD(today);
}

/*
  Function: renderCurrentLogs()
  Description:
    Displays current logs on the current page.
    Page 1 uses 6 cards.
    Page 2 and Page 3 use 4 cards.

  How to use:
    renderCurrentLogs(6);
    renderCurrentLogs(4);
*/
void renderCurrentLogs(uint8_t visibleCards) {
  activeCurrentLogCards = visibleCards;

  for (uint8_t card = 0; card < visibleCards; card++) {
    uint16_t visibleIndex = currentLogOffset + card;
    if (visibleIndex >= currentLogCount) {
      clearCurrentLogCard(card);
      continue;
    }

    uint8_t logIndex = currentLogCount - 1 - visibleIndex;
    uint16_t base = VP_LOG_BASE + (card * LOG_CARD_STEP);

    writeVPText(base + LOG_KEYNAME_OFFSET, currentLogs[logIndex].keyName.c_str(), LOG_KEYNAME_BYTES);
    writeVPText(base + LOG_NAME_OFFSET, currentLogs[logIndex].borrowerName.c_str(), LOG_NAME_BYTES);
    writeVPText(base + LOG_KEYNO_OFFSET, currentLogs[logIndex].keyNumber.c_str(), LOG_KEYNO_BYTES);
    writeVPText(base + LOG_TIME_OFFSET, currentLogs[logIndex].timeText.c_str(), LOG_TIME_BYTES);
    writeVPWord(base + LOG_STATUS_OFFSET, currentLogs[logIndex].statusIcon);
  }

  for (uint8_t card = visibleCards; card < MAX_CURRENT_LOG_CARDS; card++) {
    clearCurrentLogCard(card);
  }
}

/*
  Function: clearCurrentLogCard()
  Description:
    Clears one current log card on DWIN.

  How to use:
    clearCurrentLogCard(0);
*/
void clearCurrentLogCard(uint8_t cardIndex) {
  uint16_t base = VP_LOG_BASE + (cardIndex * LOG_CARD_STEP);

  writeVPText(base + LOG_KEYNAME_OFFSET, "", LOG_KEYNAME_BYTES);
  writeVPText(base + LOG_NAME_OFFSET, "", LOG_NAME_BYTES);
  writeVPText(base + LOG_KEYNO_OFFSET, "", LOG_KEYNO_BYTES);
  writeVPText(base + LOG_TIME_OFFSET, "", LOG_TIME_BYTES);
  writeVPWord(base + LOG_STATUS_OFFSET, LOG_STATUS_EMPTY);
}

/*
  Function: handleCurrentLogControls()
  Description:
    Handles current log scroll up/down.

  How to use:
    DGUS buttons:
      VP 0x1031 value 0x0001 = scroll up
      VP 0x1032 value 0x0001 = scroll down
*/
bool handleCurrentLogControls(uint16_t vp, uint16_t value) {
  if (vp == VP_LOG_SCROLL_UP) {
    scrollCurrentLogsUp();
    writeVPWord(VP_LOG_SCROLL_UP, 0);
    return true;
  }

  if (vp == VP_LOG_SCROLL_DOWN) {
    scrollCurrentLogsDown();
    writeVPWord(VP_LOG_SCROLL_DOWN, 0);
    return true;
  }

  return false;
}

/*
  Function: scrollCurrentLogsUp()
  Description:
    Moves current log list upward.

  How to use:
    Called by VP_LOG_SCROLL_UP.
*/
void scrollCurrentLogsUp() {
  if (currentLogOffset > 0) {
    currentLogOffset--;
    renderCurrentLogs(activeCurrentLogCards);
  }
}

/*
  Function: scrollCurrentLogsDown()
  Description:
    Moves current log list downward.

  How to use:
    Called by VP_LOG_SCROLL_DOWN.
*/
void scrollCurrentLogsDown() {
  if (currentLogOffset + activeCurrentLogCards < currentLogCount) {
    currentLogOffset++;
    renderCurrentLogs(activeCurrentLogCards);
  }
}
