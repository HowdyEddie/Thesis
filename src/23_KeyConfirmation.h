// =======================================================
// KeyConfirmation.h
// Keeps a borrowed/returned slot open until the user confirms the key action.
// =======================================================

void showKeyConfirmationPopup() {
  writeVPWord(VP_KEY_CONFIRM_POPUP, KEY_CONFIRM_POPUP_SHOW);
}

void clearKeyConfirmationPopup() {
  writeVPWord(VP_KEY_CONFIRM_POPUP, 0);
}

bool beginBorrowConfirmation(String keyNumber, String keyName,
                             String borrowerName, String studentCode,
                             String userName) {
  if (pendingLockerAction.type != LOCKER_ACTION_NONE ||
      selectedSlotIndex < 0 || selectedSlotIndex >= SLOT_COUNT) {
    debugFail("beginBorrowConfirmation", "Cannot start borrow confirmation without one available selected slot.");
    return false;
  }

  uint8_t slotIndex = static_cast<uint8_t>(selectedSlotIndex);
  if (!slotMaintenanceActive[slotIndex] || slotTaken[slotIndex] ||
      digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL) {
    debugFail("beginBorrowConfirmation", "Selected slot is unavailable or the safety limit switch is active.");
    return false;
  }

  pendingLockerAction.type = LOCKER_ACTION_BORROW;
  pendingLockerAction.slotIndex = slotIndex;
  pendingLockerAction.keyIndex = 0;
  pendingLockerAction.borrowerName = borrowerName;
  pendingLockerAction.studentCode = studentCode;
  pendingLockerAction.keyNumber = keyNumber;
  pendingLockerAction.keyName = keyName;
  pendingLockerAction.userName = userName;

  slotUnlockActive[slotIndex] = false;
  setSlotLockOutput(slotIndex, true);

  uint16_t dashboardPage =
      qrReturnPage == PAGE_HOME_DASHBOARD_B
          ? PAGE_HOME_DASHBOARD_B
          : PAGE_HOME_DASHBOARD_A;
  goToHomeDashboard(dashboardPage);

  selectedSlotIndex = slotIndex;
  slotSelected[slotIndex] = true;
  updateSlotDisplay(slotIndex);
  showKeyConfirmationPopup();
  debugNumber("beginBorrowConfirmation", "Waiting for key-take confirmation for slot", slotIndex + 1);
  return true;
}

bool confirmPendingLockerAction() {
  if (pendingLockerAction.type == LOCKER_ACTION_NONE) return false;
  if (digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL) {
    debugFail("confirmPendingLockerAction", "Confirmation ignored while the safety limit switch is active.");
    return false;
  }

  uint8_t slotIndex = pendingLockerAction.slotIndex;
  if (slotIndex >= SLOT_COUNT) {
    debugFail("confirmPendingLockerAction", "Pending slot index is invalid.");
    return false;
  }

  if (pendingLockerAction.type == LOCKER_ACTION_BORROW) {
    String dateText = getRTCDateString();
    String borrowedTime = getRTCTimeString();
    if (!addTransactionLog(
            pendingLockerAction.keyNumber,
            pendingLockerAction.borrowerName,
            pendingLockerAction.keyName,
            borrowedTime,
            "--:--",
            pendingLockerAction.userName,
            dateText,
            pendingLockerAction.studentCode)) {
      debugFail("confirmPendingLockerAction", "Borrow remains pending because its daily transaction could not be committed.");
      return false;
    }

    addCurrentLog(
        pendingLockerAction.keyName,
        pendingLockerAction.borrowerName,
        pendingLockerAction.keyNumber,
        borrowedTime,
        LOG_STATUS_BORROWED);

    slotTaken[slotIndex] = true;
    saveSlotTakenState(slotIndex);
    slotDisplayState[slotIndex] = SLOT_TAKEN;
    slotSelected[slotIndex] = false;
    slotUnlockActive[slotIndex] = false;
    setSlotLockOutput(slotIndex, false);
    updateSlotDisplay(slotIndex);
    debugNumber("confirmPendingLockerAction", "Borrow confirmed for slot", slotIndex + 1);
  } else if (pendingLockerAction.type == LOCKER_ACTION_RETURN) {
    uint8_t keyIndex = pendingLockerAction.keyIndex;
    if (!markRegisteredKeyReturned(keyIndex)) {
      debugFail("confirmPendingLockerAction", "Return remains pending because its daily transaction could not be committed.");
      return false;
    }

    lastReturnedRFIDUID =
        normalizeRFIDUID(registeredKeys[keyIndex].rfidUID);
    lastRFIDReturnAction = millis();
    debugNumber("confirmPendingLockerAction", "Return confirmed for slot", slotIndex + 1);
  } else {
    debugFail("confirmPendingLockerAction", "Unknown pending locker action.");
    return false;
  }

  cancelPendingLockerAction(false);
  return true;
}

void cancelPendingLockerAction(bool relockSlot) {
  if (pendingLockerAction.type == LOCKER_ACTION_NONE) {
    clearKeyConfirmationPopup();
    return;
  }

  uint8_t slotIndex = pendingLockerAction.slotIndex;
  pendingLockerAction = {};
  selectedSlotIndex = -1;

  if (slotIndex < SLOT_COUNT) {
    if (relockSlot) {
      slotUnlockActive[slotIndex] = false;
      setSlotLockOutput(slotIndex, false);
    }
    slotSelected[slotIndex] = false;
    updateSlotDisplay(slotIndex);
  }

  clearKeyConfirmationPopup();
  debugLog("cancelPendingLockerAction", "Pending key action cancelled without recording a transaction.");
}
