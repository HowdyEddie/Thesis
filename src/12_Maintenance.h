// =======================================================
// Maintenance.ino
// Page 11-12 maintenance: select 40 slots, set active/inactive, save.
// =======================================================

/*
  Function: initMaintenanceStateDefaults()
  Description:
    Sets all slots active and clears selections before saved data is loaded.

  How to use:
    Called in setup().
*/
void initMaintenanceStateDefaults() {
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotSelected[i] = false;
    slotTaken[i] = false;
    slotMaintenanceActive[i] = true;
    slotDisplayState[i] = SLOT_AVAILABLE;
  }
}

/*
  Function: loadMaintenanceState()
  Description:
    Loads saved active/inactive slot states from ESP32 NVS memory.

  How to use:
    Called during loading screen.
*/
void loadMaintenanceState() {
  prefs.begin("locker", true);

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    String key = "s" + String(i);
    String takenKey = "t" + String(i);
    slotMaintenanceActive[i] = prefs.getBool(key.c_str(), true);
    slotTaken[i] = prefs.getBool(takenKey.c_str(), false);
    slotDisplayState[i] = slotTaken[i] ? SLOT_TAKEN :
                          (slotMaintenanceActive[i] ? SLOT_AVAILABLE : SLOT_NA);
  }

  prefs.end();

  Serial.println("Maintenance slot state loaded.");
}

/*
  Function: saveMaintenanceState()
  Description:
    Saves active/inactive slot states so they remain after reset.

  How to use:
    Called when Save Changes button is pressed.
*/
void saveMaintenanceState() {
  prefs.begin("locker", false);

  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    String key = "s" + String(i);
    String takenKey = "t" + String(i);
    prefs.putBool(key.c_str(), slotMaintenanceActive[i]);
    prefs.putBool(takenKey.c_str(), slotTaken[i]);
  }

  prefs.end();

  writeVPText(VP_MAINT_STATUS_TEXT, "Changes saved", 32);
  writeVPWord(VP_MAINT_STATUS_ICON, ICON_CHANGES_SAVED);
  delay(1000);
  Serial.println("Maintenance slot state saved.");
  writeVPWord(VP_MAINT_STATUS_ICON, ICON_IDLE);
}

void saveSlotTakenState(uint8_t slotIndex) {
  if (slotIndex >= SLOT_COUNT) return;
  prefs.begin("locker", false);
  String key = "t" + String(slotIndex);
  prefs.putBool(key.c_str(), slotTaken[slotIndex]);
  prefs.end();
}

/*
  Function: renderMaintenanceSlots()
  Description:
    Displays all 40 slots using active/inactive/selected states.

  How to use:
    Called when Page 11 or Page 12 opens.
*/
void renderMaintenanceSlots() {
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    updateSlotDisplay(i);
  }

  writeVPText(VP_MAINT_STATUS_TEXT, "Select slot", 32);
  writeVPWord(VP_MAINT_STATUS_ICON, 0x0000);
}

/*
  Function: handleMaintenanceControls()
  Description:
    Handles Active, Inactive, Save, and Back buttons on Maintenance Page.

  How to use:
    Called automatically from handleDWIN().
*/
bool handleMaintenanceControls(uint16_t vp, uint16_t value) {
  if (!(currentPage == PAGE_MAINTENANCE_A || currentPage == PAGE_MAINTENANCE_B)) return false;

  if (vp == VP_MAINT_BACK) {
    writeVPWord(VP_MAINT_BACK, 0);
    goToAdminDashboard();
    return true;
  }

  if (vp == VP_MAINT_SET_ACTIVE) {
    writeVPWord(VP_MAINT_SET_ACTIVE, 0);
    setSelectedSlotsActive(true);
    return true;
  }

  if (vp == VP_MAINT_SET_INACTIVE) {
    writeVPWord(VP_MAINT_SET_INACTIVE, 0);
    setSelectedSlotsActive(false);
    return true;
  }

  if (vp == VP_MAINT_SAVE_CHANGES) {
    writeVPWord(VP_MAINT_SAVE_CHANGES, 0);
    saveMaintenanceState();
    return true;
  }

  return false;
}

/*
  Function: handleSlotControls()
  Description:
    Handles Slot 1 to Slot 40 touch events and the shared OK/Confirm button.

    Home Dashboard behavior:
      Tap an available slot to select it. Tap that same slot again to
      deselect it; tapping another available slot moves the selection.
      Press OK/Confirm VP 0x1028 -> open Page 4.

    Page 4 QR/Borrower behavior:
      Press OK/Confirm VP 0x1028 -> confirm borrower name and log transaction.

    Maintenance behavior:
      Slots can be multi-selected so Active/Inactive can be applied to many slots.

  How to use:
    Slot VP range:
      Slot 1 = 0x1000
      Slot 40 = 0x1027

    Confirm button:
      VP = 0x1028
      Value = 0x0001
*/
bool handleSlotControls(uint16_t vp, uint16_t value) {
  bool onHomeDashboard =
      currentPage == PAGE_HOME_DASHBOARD_A ||
      currentPage == PAGE_HOME_DASHBOARD_B;
  bool isSlotVP = vp >= SLOT_BASE_VP && vp < SLOT_BASE_VP + SLOT_COUNT;

  if (onHomeDashboard &&
      (vp == VP_SLOT_CONFIRM || isSlotVP) &&
      digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL) {
    if (isSlotVP) {
      updateSlotDisplay(vp - SLOT_BASE_VP);
    } else {
      writeVPWord(VP_SLOT_CONFIRM, 0);
    }
    Serial.println("Slot input ignored while the safety limit switch is active.");
    return true;
  }

  if (vp == VP_SLOT_CONFIRM) {
    writeVPWord(VP_SLOT_CONFIRM, 0);

    if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
      if (selectedSlotIndex < 0) {
        Serial.println("No slot selected. Select a slot before pressing OK.");
        return true;
      }

      Serial.print("Confirmed selected slot: ");
      Serial.println(selectedSlotIndex + 1);

      goToQRBorrowerPage();
      return true;
    }

    if (currentPage == PAGE_QR_BORROWER) {
      confirmBorrowerIdentity();
      return true;
    }

    return true;
  }

  if (vp == VP_SLOT_CANCEL) {
    writeVPWord(VP_SLOT_CANCEL, 0);

    if (currentPage == PAGE_QR_BORROWER) {
      cancelQRBorrowerPage();
    } else {
      clearSlotSelections();
      selectedSlotIndex = -1;
    }

    return true;
  }

  if (vp < SLOT_BASE_VP || vp >= SLOT_BASE_VP + SLOT_COUNT) return false;

  uint8_t slotIndex = vp - SLOT_BASE_VP;

  if (currentPage == PAGE_MAINTENANCE_A || currentPage == PAGE_MAINTENANCE_B) {
    slotSelected[slotIndex] = !slotSelected[slotIndex];
    updateSlotDisplay(slotIndex);

    Serial.print("Maintenance slot toggled: ");
    Serial.println(slotIndex + 1);

    return true;
  }

  if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
    if (!slotMaintenanceActive[slotIndex]) {
      Serial.println("Slot is inactive.");
      writeVPWord(vp, SLOT_NA);
      return true;
    }

    if (slotTaken[slotIndex]) {
      Serial.println("Slot is already taken.");
      writeVPWord(vp, SLOT_TAKEN);
      return true;
    }

    bool deselectCurrentSlot = slotSelected[slotIndex];
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
      slotSelected[i] = false;
      updateSlotDisplay(i);
    }

    if (deselectCurrentSlot) {
      selectedSlotIndex = -1;
      Serial.print("Deselected slot: ");
      Serial.println(slotIndex + 1);
      return true;
    }

    slotSelected[slotIndex] = true;
    selectedSlotIndex = slotIndex;
    updateSlotDisplay(slotIndex);

    Serial.print("Slot VP touch accepted (value 0x");
    Serial.print(value, HEX);
    Serial.println("); value zero is valid for slot selection.");
    Serial.print("Selected slot: ");
    Serial.println(selectedSlotIndex + 1);

    return true;
  }

  return true;
}

/*
  Function: setSelectedSlotsActive()
  Description:
    Sets selected maintenance slots active or inactive.

  How to use:
    setSelectedSlotsActive(true);
    setSelectedSlotsActive(false);
*/
void setSelectedSlotsActive(bool active) {
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    if (slotSelected[i]) {
      slotMaintenanceActive[i] = active;
      slotSelected[i] = false;
      slotDisplayState[i] = slotTaken[i] ? SLOT_TAKEN :
                            (active ? SLOT_AVAILABLE : SLOT_NA);
      updateSlotDisplay(i);
    }
  }

  //writeVPText(VP_MAINT_STATUS_TEXT, active ? "Set active" : "Set inactive", 32);
  //writeVPWord(VP_MAINT_STATUS_ICON, 0x0000);
}

/*
  Function: updateSlotDisplay()
  Description:
    Writes the correct icon/state value to a slot VP.

  How to use:
    updateSlotDisplay(0); // Slot 1
*/
void updateSlotDisplay(uint8_t slotIndex) {
  uint16_t vp = SLOT_BASE_VP + slotIndex;

  if (slotSelected[slotIndex]) {
    writeVPWord(vp, SLOT_SELECTED);
  } else if (!slotMaintenanceActive[slotIndex]) {
    writeVPWord(vp, SLOT_NA);
  } else if (slotTaken[slotIndex]) {
    writeVPWord(vp, SLOT_TAKEN);
  } else {
    writeVPWord(vp, slotDisplayState[slotIndex]);
  }
}

/*
  Function: clearSlotSelections()
  Description:
    Clears selected slot states.

  How to use:
    Call after confirm/cancel.
*/
void clearSlotSelections() {
  for (uint8_t i = 0; i < SLOT_COUNT; i++) {
    slotSelected[i] = false;
    updateSlotDisplay(i);
  }
}
