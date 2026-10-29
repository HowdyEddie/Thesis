// =======================================================
// TransactionLogs.ino
// Admin dashboard 11-row transaction log and 6 date cards.
// =======================================================

/*
  Function: initTransactionLogs()
  Description:
    Clears transaction-log RAM during startup.
    Real transactions are added by addTransactionLog().

  How to use:
    Called during loading screen.
*/
void initTransactionLogs() {
  transactionLogCount = 0;
  transactionLogOffset = 0;
}

/*
  Function: initDateLogs()
  Description:
    Initializes the Admin Dashboard date-card list using the current RTC date.

  How to use:
    Called during loading screen.
*/
void initDateLogs() {
  dateLogCount = 0;
  dateCardOffset = 0;
  selectedDateVisibleIndex = -1;
  selectedDateText = "";

  String today = getRTCDateString();

  if (today != "0000-00-00") {
    addDateLogIfMissing(today);
    selectedDateText = today;
    selectedDateVisibleIndex = 0;
  }
}

/*
  Function: addDateLogIfMissing()
  Description:
    Adds a date to the date-card list if it is not already present.

  How to use:
    Called when a transaction is logged.
*/
void addDateLogIfMissing(String dateText) {
  if (dateText.length() == 0) return;

  for (uint8_t i = 0; i < dateLogCount; i++) {
    if (dateLogs[i] == dateText) return;
  }

  if (dateLogCount >= MAX_DATE_LOGS) return;

  dateLogs[dateLogCount] = dateText;
  dateLogCount++;
}

/*
  Function: addTransactionLog()
  Description:
    Commits the transaction to the authoritative daily SD archive before
    adding it to RAM. The RAM list remains chronological; rendering presents
    newest-first.

  How to use:
    Called before slot state or output changes for a borrow/return action.
*/
bool addTransactionLog(String keyNo, String borrowerName, String keyName,
                      String borrowedTime, String returnedTime,
                      String userName, String dateText, String studentCode) {
  String csvLine = dateText + "," + csvEscapeField(keyNo) + "," +
                   csvEscapeField(borrowerName) + "," + csvEscapeField(keyName) + "," +
                   csvEscapeField(borrowedTime) + "," + csvEscapeField(returnedTime) + "," +
                   csvEscapeField(userName) + "," + csvEscapeField(studentCode);

  if (!appendLineToDaySD("/transactions.csv", csvLine)) {
    debugFail("addTransactionLog", "Daily transaction was not committed; locker state was not changed.");
    return false;
  }

  if (transactionLogCount >= MAX_TRANSACTION_LOGS) {
    for (uint8_t i = 1; i < MAX_TRANSACTION_LOGS; i++) {
      transactionLogs[i - 1] = transactionLogs[i];
    }
    transactionLogCount = MAX_TRANSACTION_LOGS - 1;
  }

  transactionLogs[transactionLogCount].keyNo = keyNo;
  transactionLogs[transactionLogCount].borrowerName = borrowerName;
  transactionLogs[transactionLogCount].keyName = keyName;
  transactionLogs[transactionLogCount].borrowedTime = borrowedTime;
  transactionLogs[transactionLogCount].returnedTime = returnedTime;
  transactionLogs[transactionLogCount].userName = userName;
  transactionLogs[transactionLogCount].dateText = dateText;
  transactionLogCount++;

  addDateLogIfMissing(dateText);

  if (currentPage == PAGE_ADMIN_DASHBOARD) {
    if (selectedDateText == dateText) {
      transactionLogOffset = 0;
    } else {
      uint16_t maxOffset = transactionLogCount > TRANS_VISIBLE_ROWS
          ? transactionLogCount - TRANS_VISIBLE_ROWS
          : 0;
      if (transactionLogOffset < maxOffset) transactionLogOffset++;
    }
    renderTransactionLogs();
    renderDateCards();
  }

  return true;
}

/*
  Function: renderTransactionLogs()
  Description:
    Displays 11 visible transaction log rows on Page 7.

  How to use:
    Called when Admin Dashboard opens or when scrolling.
*/
void renderTransactionLogs() {
  for (uint8_t row = 0; row < TRANS_VISIBLE_ROWS; row++) {
    uint16_t visibleIndex = transactionLogOffset + row;
    if (visibleIndex >= transactionLogCount) {
      clearTransactionRow(row);
      continue;
    }

    uint8_t logIndex = transactionLogCount - 1 - visibleIndex;
    uint16_t base = VP_TRANS_BASE + (row * TRANS_ROW_STEP);

    writeVPText(base + TRANS_KEYNO_OFFSET, transactionLogs[logIndex].keyNo.c_str(), TRANS_KEYNO_BYTES);
    writeVPText(base + TRANS_BORROWER_OFFSET, transactionLogs[logIndex].borrowerName.c_str(), TRANS_BORROWER_BYTES);
    writeVPText(base + TRANS_KEYNAME_OFFSET, transactionLogs[logIndex].keyName.c_str(), TRANS_KEYNAME_BYTES);
    writeVPText(base + TRANS_BORROWED_OFFSET, transactionLogs[logIndex].borrowedTime.c_str(), TRANS_BORROWED_BYTES);
    writeVPText(base + TRANS_RETURNED_OFFSET, transactionLogs[logIndex].returnedTime.c_str(), TRANS_RETURNED_BYTES);
    writeVPText(base + TRANS_USER_OFFSET, transactionLogs[logIndex].userName.c_str(), TRANS_USER_BYTES);
  }
}

/*
  Function: clearTransactionRow()
  Description:
    Clears one transaction row.

  How to use:
    clearTransactionRow(0);
*/
void clearTransactionRow(uint8_t rowIndex) {
  uint16_t base = VP_TRANS_BASE + (rowIndex * TRANS_ROW_STEP);

  writeVPText(base + TRANS_KEYNO_OFFSET, "", TRANS_KEYNO_BYTES);
  writeVPText(base + TRANS_BORROWER_OFFSET, "", TRANS_BORROWER_BYTES);
  writeVPText(base + TRANS_KEYNAME_OFFSET, "", TRANS_KEYNAME_BYTES);
  writeVPText(base + TRANS_BORROWED_OFFSET, "", TRANS_BORROWED_BYTES);
  writeVPText(base + TRANS_RETURNED_OFFSET, "", TRANS_RETURNED_BYTES);
  writeVPText(base + TRANS_USER_OFFSET, "", TRANS_USER_BYTES);
}

/*
  Function: handleTransactionLogControls()
  Description:
    Handles Admin Dashboard transaction log scroll buttons.

  How to use:
    VP 0x1310 = up
    VP 0x1311 = down
*/
bool handleTransactionLogControls(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_ADMIN_DASHBOARD) return false;

  if (vp == VP_TRANS_SCROLL_UP) {
    scrollTransactionLogsUp();
    writeVPWord(VP_TRANS_SCROLL_UP, 0);
    return true;
  }

  if (vp == VP_TRANS_SCROLL_DOWN) {
    scrollTransactionLogsDown();
    writeVPWord(VP_TRANS_SCROLL_DOWN, 0);
    return true;
  }

  return false;
}

/*
  Function: scrollTransactionLogsUp()
  Description:
    Scrolls toward newer transaction rows.

  How to use:
    Called by VP_TRANS_SCROLL_UP.
*/
void scrollTransactionLogsUp() {
  if (transactionLogOffset > 0) {
    transactionLogOffset--;
    renderTransactionLogs();
  }
}

/*
  Function: scrollTransactionLogsDown()
  Description:
    Scrolls toward older transaction rows.

  How to use:
    Called by VP_TRANS_SCROLL_DOWN.
*/
void scrollTransactionLogsDown() {
  if (transactionLogOffset + TRANS_VISIBLE_ROWS < transactionLogCount) {
    transactionLogOffset++;
    renderTransactionLogs();
  }
}

/*
  Function: renderDateCards()
  Description:
    Displays 6 scrollable date cards on Admin Dashboard.

  How to use:
    Called when Page 7 opens or when date cards scroll.
*/
void renderDateCards() {
  for (uint8_t card = 0; card < DATE_CARD_COUNT; card++) {
    uint8_t index = dateCardOffset + card;
    uint16_t vp = VP_DATE_CARD_BASE + (card * DATE_CARD_STEP);

    if (index < dateLogCount) {
      writeVPText(vp, dateLogs[index].c_str(), DATE_CARD_BYTES);
    } else {
      writeVPText(vp, "", DATE_CARD_BYTES);
    }
  }
}

/*
  Function: handleDateCards()
  Description:
    Handles date-card scroll and select buttons.

  How to use:
    VP 0x34D0 value 0x0001 = date scroll up
    VP 0x34D1 value 0x0001 = date scroll down
    VP 0x34D2 value 1-6    = select visible date card
*/
bool handleDateCards(uint16_t vp, uint16_t value) {
  if (currentPage != PAGE_ADMIN_DASHBOARD) return false;

  if (vp == VP_DATE_SCROLL_UP) {
    if (dateCardOffset > 0) dateCardOffset--;
    renderDateCards();
    writeVPWord(VP_DATE_SCROLL_UP, 0);
    return true;
  }

  if (vp == VP_DATE_SCROLL_DOWN) {
    if (dateCardOffset + DATE_CARD_COUNT < dateLogCount) dateCardOffset++;
    renderDateCards();
    writeVPWord(VP_DATE_SCROLL_DOWN, 0);
    return true;
  }

  if (vp == VP_DATE_SELECT) {
    if (value >= 1 && value <= DATE_CARD_COUNT) {
      uint8_t selectedIndex = dateCardOffset + (value - 1);

      if (selectedIndex < dateLogCount) {
        selectedDateVisibleIndex = value - 1;
        selectedDateText = dateLogs[selectedIndex];

        Serial.print("Selected date: ");
        Serial.println(selectedDateText);
      }
    }

    writeVPWord(VP_DATE_SELECT, 0);
    return true;
  }

  return false;
}
