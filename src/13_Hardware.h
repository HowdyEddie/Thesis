// =======================================================
// Hardware.ino
// Initializes pins, MCP23017 expanders, and slot lock outputs.
// =======================================================

/*
  Function: initHardwarePins()
  Description:
    Sets chip-select pins and default output states.

  How to use:
    Called in setup().
*/
void initHardwarePins() {
  debugLog("initHardwarePins", "Setting chip-select pins to OUTPUT/HIGH.");

  pinMode(RFID_CS, OUTPUT);
  pinMode(SD_STORAGE_CS, OUTPUT);
  pinMode(USB_CS, OUTPUT);

  digitalWrite(RFID_CS, HIGH);
  digitalWrite(SD_STORAGE_CS, HIGH);
  digitalWrite(USB_CS, HIGH);
  
  delay(100);
  //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  debugOK("initHardwarePins");
}

/*
  Function: initIOExpanders()
  Description:
    Starts the 3 CJMCU-2317/MCP23017 I2C expanders.

  How to use:
    Called during loading screen.
*/
bool initIOExpanders() {
  debugLog("initIOExpanders", "Starting MCP23017 detection.");

  bool ok1 = mcp1.begin_I2C(MCP1_ADDR, &Wire);
  bool ok2 = mcp2.begin_I2C(MCP2_ADDR, &Wire);
  bool ok3 = mcp3.begin_I2C(MCP3_ADDR, &Wire);

  debugValue("initIOExpanders", "MCP1 0x20", ok1 ? "FOUND" : "NOT FOUND");
  debugValue("initIOExpanders", "MCP2 0x21", ok2 ? "FOUND" : "NOT FOUND");
  debugValue("initIOExpanders", "MCP3 0x22", ok3 ? "FOUND" : "NOT FOUND");

  mcpReady = ok1 && ok2 && ok3;

  if (!mcpReady) {
    debugFail("initIOExpanders", "One or more MCP23017 expanders not found.");
    return false;
  }

  for (uint8_t pin = 0; pin < 16; pin++) {
    mcp1.pinMode(pin, OUTPUT);
    mcp2.pinMode(pin, OUTPUT);
    mcp3.pinMode(pin, OUTPUT);

    mcp1.digitalWrite(pin, LOCK_INACTIVE_LEVEL);
    mcp2.digitalWrite(pin, LOCK_INACTIVE_LEVEL);
    mcp3.digitalWrite(pin, LOCK_INACTIVE_LEVEL);
  }

  debugOK("initIOExpanders");
  return true;
}

/*
  Function: setSlotLockOutput()
  Description:
    Controls one slot lock output using the MCP23017 expanders.

  How to use:
    setSlotLockOutput(0, true);  // Unlock slot 1
    setSlotLockOutput(0, false); // Lock slot 1
*/
void setSlotLockOutput(uint8_t slotIndex, bool unlocked) {
  if (!mcpReady) {
    debugFail("setSlotLockOutput", "MCP expanders not ready.");
    return;
  }

  if (slotIndex >= SLOT_COUNT) {
    debugNumber("setSlotLockOutput", "Invalid slot index", slotIndex);
    return;
  }

  uint8_t expanderIndex = slotIndex / 16;
  uint8_t pinIndex = slotIndex % 16;
  uint8_t level = unlocked ? LOCK_ACTIVE_LEVEL : LOCK_INACTIVE_LEVEL;

  debugNumber("setSlotLockOutput", "Slot index", slotIndex);
  debugNumber("setSlotLockOutput", "Expander index", expanderIndex);
  debugNumber("setSlotLockOutput", "Pin index", pinIndex);
  debugValue("setSlotLockOutput", "Action", unlocked ? "UNLOCK/ACTIVE" : "LOCK/INACTIVE");

  if (expanderIndex == 0) {
    mcp1.digitalWrite(pinIndex, level);
  }
  else if (expanderIndex == 1) {
    mcp2.digitalWrite(pinIndex, level);
  }
  else {
    mcp3.digitalWrite(pinIndex, level);
  }
}
