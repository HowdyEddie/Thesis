/*
  DWIN Smart Locker Full Modular Final Integrated Code
  Board: ESP32 DevKit V4
  Display: DWIN DMG80480Y070_02NR
  Project style:
    - Main config and globals are in this file.
    - Feature implementation headers are included once below.

  Build this PlatformIO project for the esp32dev environment.

  Important:
    Install these libraries in Arduino IDE Library Manager when needed:
      - Adafruit MCP23017 Arduino Library / Adafruit MCP23X17
      - Adafruit Fingerprint Sensor Library
      - MFRC522
      - RTClib
      - EspSoftwareSerial or SoftwareSerial for ESP32 QR scanner
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Preferences.h>

#include <Adafruit_MCP23X17.h>
#include <Adafruit_Fingerprint.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <Ch376msc.h>

// QR scanner SoftwareSerial
// This is intentionally forced instead of using __has_include().
// Arduino IDE sometimes does not add the EspSoftwareSerial library path
// when the include is hidden inside __has_include().
#include <SoftwareSerial.h>
#define HAS_QR_SOFTWARE_SERIAL 1

// =======================================================
// DEBUG SERIAL CONFIG
// =======================================================

// Set to 0 after final testing if you want quieter Serial Monitor output.
#define DEBUG_SERIAL 1

// =======================================================
// PIN MAP
// =======================================================

// DWIN Display UART2
#define DWIN_RX    16
#define DWIN_TX    17
#define DWIN_BAUD  115200

// Capacitive FPS R503 UART1
#define FPS_RX     26
#define FPS_TX     27
#define FPS_BAUD   57600

// GM861S/B1 QR Scanner using Software Serial
#define QR_RX      32
#define QR_TX      33
#define QR_BAUD    9600

// I2C bus for 3pcs CJMCU-2317 / MCP23017
#define I2C_SDA    21
#define I2C_SCL    22

#define MCP1_ADDR  0x20
#define MCP2_ADDR  0x21
#define MCP3_ADDR  0x22

// Shared SPI bus
#define SPI_SCK    18
#define SPI_MISO   19
#define SPI_MOSI   23

#define RFID_CS    5
#define RFID_RST   4

#define SD_STORAGE_CS  14
#define USB_CS       25   // CH376S SPI chip select
#define USB_INT      35   // CH376S active-low interrupt; GPIO35 needs external pull-up

// One limit switch safety input.
// GPIO34 is input-only and available in this project.
// Wiring for active HIGH: switch to 3.3V, external 10k pulldown to GND.
#define LIMIT_SWITCH_PIN            34
#define LIMIT_SWITCH_ACTIVE_LEVEL   HIGH
#define LIMIT_SWITCH_CHECK_INTERVAL 50

// Slot output behavior through MCP23017 expanders.
// In this project: HIGH = unlock/active, LOW = lock/inactive.
#define LOCK_ACTIVE_LEVEL    HIGH
#define LOCK_INACTIVE_LEVEL  LOW

// =======================================================
// PAGE MAP
// Page numbers are recorded exactly as planned, even if they jump.
// =======================================================

#define PAGE_LOADING             0
#define PAGE_LOCKED_SCREEN       1
#define PAGE_HOME_DASHBOARD_A    2
#define PAGE_HOME_DASHBOARD_B    3
#define PAGE_QR_BORROWER         4
#define PAGE_ADMIN_LOGIN         5
#define PAGE_ADMIN_DASHBOARD     7
#define PAGE_ACCOUNT             8
#define PAGE_REGISTRATION        9
#define PAGE_NEW_KEY_REG         10
#define PAGE_MAINTENANCE_A       11
#define PAGE_MAINTENANCE_B       12
#define PAGE_UPDATE_PASSWORD      27

// Custom keyboard pages from your adjusted code
#define PAGE_KEYBOARD_LOWER      13
#define PAGE_KEYBOARD_NUMBER     14
#define PAGE_KEYBOARD_UPPER      28

// Compatibility names from earlier progress
#define PAGE_HOME_6_LOGS         PAGE_LOCKED_SCREEN
#define PAGE_CURRENT_LOG_4       PAGE_HOME_DASHBOARD_A

// =======================================================
// RTC ONE-TIME MANUAL SETUP
// =======================================================
//
// To set the DS3231 RTC once, change SET_RTC_FIXED_ONCE to 1,
// upload once, then change it back to 0 and upload again.
//
// Format:
//   Year, Month, Day, Hour, Minute, Second
#define SET_RTC_FIXED_ONCE       0
#define RTC_FIXED_YEAR           2026
#define RTC_FIXED_MONTH          7
#define RTC_FIXED_DAY            16
#define RTC_FIXED_HOUR           14
#define RTC_FIXED_MINUTE         30
#define RTC_FIXED_SECOND         0

// =======================================================
// DWIN BACKLIGHT + IDLE SLEEP CONFIG
// =======================================================
// Many DWIN DGUS/T5L displays use 0x0082 for backlight control.
// 100 = full brightness, 0 = backlight off.
#define DWIN_BACKLIGHT_VP          0x0082
#define DWIN_BACKLIGHT_FULL        100
#define DWIN_BACKLIGHT_OFF         0

// If no user activity is detected for this long, ESP32 returns to
// Locked Screen and turns off/dims the DWIN backlight.
#define DISPLAY_IDLE_TIMEOUT_MS    30000UL

// Optional DWIN touch object for waking the display when the backlight is off.
// Add a Return Key Code / Basic Touch object on the Locked Screen using this VP.
#define VP_DISPLAY_WAKE_TOUCH     0x13F0


// =======================================================
// DWIN VP MAP
// =======================================================

// Slots
#define SLOT_BASE_VP        0x1000
#define SLOT_COUNT          40

#define VP_SLOT_CONFIRM     0x1028
#define VP_SLOT_CANCEL      0x1029
#define VP_SELECTED_COUNT   0x1030

#define SLOT_AVAILABLE      0x0000
#define SLOT_SELECTED       0x0001
#define SLOT_TAKEN          0x0002
#define SLOT_NA             0x0003

// Home Dashboard slot key-name text displays.
// Add one DWIN Text Display per slot using these VPs.
// Each field is 32 bytes / 16 DGUS words.
#define VP_SLOT_KEY_NAME_BASE    0x2000
#define SLOT_KEY_NAME_STEP       0x20
#define SLOT_KEY_NAME_BYTES      32


// Current logs
#define VP_LOG_SCROLL_UP    0x1031
#define VP_LOG_SCROLL_DOWN  0x1032

#define VP_LOG_BASE         0x1050
#define LOG_CARD_STEP       0x50

#define CURRENT_LOG_CARDS_6 6
#define CURRENT_LOG_CARDS_4 4
#define MAX_CURRENT_LOG_CARDS 6
#define MAX_CURRENT_LOGS    40

#define LOG_KEYNAME_OFFSET  0x00
#define LOG_NAME_OFFSET     0x08
#define LOG_KEYNO_OFFSET    0x18
#define LOG_TIME_OFFSET     0x20
#define LOG_STATUS_OFFSET   0x2C

#define LOG_KEYNAME_BYTES   16
#define LOG_NAME_BYTES      32
#define LOG_KEYNO_BYTES     16
#define LOG_TIME_BYTES      24

#define LOG_STATUS_EMPTY    0x0000
#define LOG_STATUS_BORROWED 0x0004
#define LOG_STATUS_RETURNED 0x0005

// Admin
#define VP_ADMIN_PIN_INPUT     0x1040
#define VP_ADMIN_OPEN          0x1042
#define VP_ADMIN_LOGIN         0x1043
#define VP_ADMIN_BACK          0x1044
#define VP_ADMIN_ACCOUNT       0x1045
#define VP_ADMIN_MAINTENANCE   0x1046
#define VP_ADMIN_DOWNLOAD      0x1047

#define VP_ADMIN_LOGIN_STATUS_TEXT  0x1660
#define VP_ADMIN_LOGIN_STATUS_ICON  0x1680

#define ADMIN_PASS_VALUE       0x3030  // Legacy/default numeric value. New password feature uses ADMIN_DEFAULT_PIN_TEXT.
#define ADMIN_DEFAULT_PIN_TEXT  "3030"
#define ADMIN_PIN_DIGITS        4
#define ADMIN_BUTTON_VALUE     0x0001

#define ADMIN_LOGIN_ICON_IDLE       0x0000
#define ADMIN_LOGIN_ICON_WAITING    0x0001
#define ADMIN_LOGIN_ICON_GRANTED    0x0002
#define ADMIN_LOGIN_ICON_DENIED     0x0003

// Loading, page command, transaction scroll
#define VP_PROGRESS_ICON       0x1300
#define VP_PAGE_CMD            0x1301

#define VP_TRANS_SCROLL_UP     0x1310
#define VP_TRANS_SCROLL_DOWN   0x1311

// Custom keyboard
#define VP_KEYBOARD_TEXT       0x1400
#define VP_KEYBOARD_CMD        0x1450
#define KEYBOARD_FIELD_BYTES   32

#define KEY_BACKSPACE          0x0008
#define KEY_ENTER              0x000D
#define KEY_SPACE              0x0020
#define KEY_SHIFT              0xF001
#define KEY_CLEAR              0xF002
#define KEY_LOWER              0x0001
#define KEY_UPPER              0x0002
#define KEY_NUMBER             0x0003

// RTC display on every page
#define VP_RTC_DATE_DISPLAY    0x1500
#define VP_RTC_TIME_DISPLAY    0x1510
#define RTC_DATE_BYTES         16
#define RTC_TIME_BYTES         16

// Page 4 QR / Borrower Identification page
// Manual borrower name uses the same keyboard text VP: 0x1400.
#define VP_QR_NAME_TEXT           VP_KEYBOARD_TEXT
#define VP_QR_STATUS_TEXT         0x1600
#define VP_QR_SELECTED_SLOT_TEXT  0x1620
#define VP_QR_STUDENT_CODE_TEXT   0x1630

#define VP_QR_NAME_BUTTON         0x1640
#define VP_QR_SCAN_BUTTON         0x1641
#define VP_QR_CANCEL              0x1642
#define VP_QR_STATUS_ICON         0x1650

#define QR_ICON_IDLE              0x0000
#define QR_ICON_WAITING           0x0001
#define QR_ICON_SCANNED           0x0002
#define QR_ICON_MANUAL_REQUIRED   0x0003
#define QR_ICON_CONFIRMED         0x0004
#define QR_ICON_ERROR             0x0005

// Transaction log table
#define VP_TRANS_BASE          0x3000
#define TRANS_ROW_STEP         0x50
#define TRANS_VISIBLE_ROWS     11
#define MAX_TRANSACTION_LOGS   120

#define TRANS_KEYNO_OFFSET     0x00
#define TRANS_BORROWER_OFFSET  0x08
#define TRANS_KEYNAME_OFFSET   0x18
#define TRANS_BORROWED_OFFSET  0x28
#define TRANS_RETURNED_OFFSET  0x30
#define TRANS_USER_OFFSET      0x38

#define TRANS_KEYNO_BYTES      16
#define TRANS_BORROWER_BYTES   32
#define TRANS_KEYNAME_BYTES    32
#define TRANS_BORROWED_BYTES   16
#define TRANS_RETURNED_BYTES   16
#define TRANS_USER_BYTES       32

// Admin date cards
#define VP_DATE_CARD_BASE      0x3400
#define DATE_CARD_STEP         0x20
#define DATE_CARD_COUNT        6
#define MAX_DATE_LOGS          60

#define VP_DATE_SCROLL_UP      0x34D0
#define VP_DATE_SCROLL_DOWN    0x34D1
#define VP_DATE_SELECT         0x34D2
#define VP_DOWNLOAD_ICON       0x34E0

#define DATE_CARD_BYTES        16

// Account page
#define VP_ACCOUNT_CARD_BASE        0x3600
#define ACCOUNT_CARD_STEP           0x50
#define ACCOUNT_VISIBLE_CARDS       6
#define MAX_ACCOUNTS                60
#define MAX_KEYS                    40

#define ACC_USERNO_OFFSET           0x00
#define ACC_USERNAME_OFFSET         0x08
#define ACC_FINGERID_OFFSET         0x18
#define ACC_DATE_OFFSET             0x20

#define ACC_USERNO_BYTES            16
#define ACC_USERNAME_BYTES          32
#define ACC_FINGERID_BYTES          16
#define ACC_DATE_BYTES              16

#define VP_ACCOUNT_SCROLL_UP        0x37F0
#define VP_ACCOUNT_SCROLL_DOWN      0x37F1
#define VP_ACCOUNT_REGISTER         0x37F2
#define VP_ACCOUNT_UPDATE_PASSWORD  0x37F3
#define VP_ACCOUNT_BACK             0x37F4
#define VP_ACCOUNT_DELETE           0x37F5

// Page 27 Update Password
// Password/PIN is exactly 4 numeric digits.
#define VP_UPDATE_PASS_OLD_TEXT       0x3B00
#define VP_UPDATE_PASS_NEW_TEXT       0x3B10
#define VP_UPDATE_PASS_CONFIRM_TEXT   0x3B20
#define VP_UPDATE_PASS_OLD_BUTTON     0x3B30
#define VP_UPDATE_PASS_NEW_BUTTON     0x3B31
#define VP_UPDATE_PASS_CONFIRM_BUTTON 0x3B32
#define VP_UPDATE_PASS_SAVE           0x3B33
#define VP_UPDATE_PASS_BACK           0x3B34
#define VP_UPDATE_PASS_STATUS_TEXT    0x3B40
#define VP_UPDATE_PASS_STATUS_ICON    0x3B60
#define UPDATE_PASS_FIELD_BYTES       8
#define UPDATE_PASS_STATUS_BYTES      32

#define UPDATE_PASS_ICON_IDLE         0x0000
#define UPDATE_PASS_ICON_EDITING      0x0001
#define UPDATE_PASS_ICON_SAVED        0x0002
#define UPDATE_PASS_ICON_ERROR        0x0003

// Registration page
#define VP_REGISTER_NAME_TEXT       0x3800
#define VP_REGISTER_NAME_BUTTON     0x3840
#define VP_REGISTER_OK              0x3841
#define VP_REGISTER_ADD_NEW_KEY     0x3842
#define VP_REGISTER_BACK            0x3843
#define VP_REGISTER_STATUS_TEXT     0x3850
#define VP_REGISTER_STATUS_ICON     0x3860

// New key registration
#define VP_KEYREG_KEY_NAME_TEXT     0x3900
#define VP_KEYREG_KEY_NO_TEXT       0x3920
#define VP_KEYREG_RFID_TEXT         0x3930
#define VP_KEYREG_KEY_NAME_BUTTON   0x3940
#define VP_KEYREG_KEY_NO_BUTTON     0x3941
#define VP_KEYREG_OK                0x3942
#define VP_KEYREG_BACK              0x3943
#define VP_KEYREG_STATUS_TEXT       0x3950
#define VP_KEYREG_STATUS_ICON       0x3960

#define KEYREG_RFID_BYTES           16
#define KEYREG_RFID_WAIT_MS         10000

// Maintenance page
#define VP_MAINT_BACK               0x3A00
#define VP_MAINT_SET_ACTIVE         0x3A01
#define VP_MAINT_SET_INACTIVE       0x3A02
#define VP_MAINT_SAVE_CHANGES       0x3A03
#define VP_MAINT_STATUS_TEXT        0x3A10
#define VP_MAINT_STATUS_ICON        0x3A20

// Variable icon common values
/*
#define ICON_IDLE                   0x0000
#define ICON_BUSY                   0x0001
#define ICON_SUCCESS                0x0002
#define ICON_FAILED                 0x0003
*/
// Key Registration Icon Values
#define ICON_IDLE                   0x0000
#define ICON_SCAN_RFID              0x0001
#define ICON_CHANGES_SAVED          0x0002  // Maintenance Icon Saved.
#define ICON_KEY_SAVED              0x0004
#define ICON_KEY_FAILED             0x0005

// RFID key-return behavior
#define RFID_RETURN_SCAN_INTERVAL   300
#define RFID_RETURN_REPEAT_GUARD_MS 5000
#define RETURN_UNLOCK_MS            5000


// Download icon values
#define DOWNLOAD_IDLE               0x0000
#define DOWNLOAD_USB_CHECKING       0x0001
#define DOWNLOAD_USB_CONNECTED      0x0002
#define DOWNLOAD_IN_PROGRESS        0x0003
#define DOWNLOAD_SUCCESS            0x0004
#define DOWNLOAD_USB_NOT_FOUND      0x0005
#define DOWNLOAD_FAILED             0x0006

// Registration icon values
#define REG_IDLE                    0x0000
#define REG_PLACE_FINGER            0x0001
#define REG_REMOVE_FINGER           0x0002
#define REG_PLACE_AGAIN             0x0003
#define REG_REGISTERING             0x0004
#define REG_SUCCESS                 0x0005
#define REG_FAILED                  0x0006

// =======================================================
// STRUCTS
// =======================================================

struct CurrentLogEntry {
  String keyName;
  String borrowerName;
  String keyNumber;
  String timeText;
  uint16_t statusIcon;
};

struct TransactionLogEntry {
  String keyNo;
  String borrowerName;
  String keyName;
  String borrowedTime;
  String returnedTime;
  String userName;
  String dateText;
};

struct AccountEntry {
  String userNumber;
  String userName;
  uint16_t fingerprintID;
  String dateRegistered;
  bool used;
};

struct KeyEntry {
  uint8_t slotNumber;
  String keyName;
  String keyNumber;
  String rfidUID;
  bool used;
};

enum KeyboardTarget {
  KB_TARGET_NONE,
  KB_TARGET_REGISTER_USER_NAME,
  KB_TARGET_KEY_NAME,
  KB_TARGET_KEY_NUMBER,
  KB_TARGET_ADMIN_PIN,
  KB_TARGET_BORROWER_NAME,
  KB_TARGET_UPDATE_OLD_PIN,
  KB_TARGET_UPDATE_NEW_PIN,
  KB_TARGET_UPDATE_CONFIRM_PIN
};

// =======================================================
// HARDWARE OBJECTS
// =======================================================

HardwareSerial DWIN(2);
HardwareSerial FPSSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FPSSerial);

RTC_DS3231 rtc;
MFRC522 rfid(RFID_CS, RFID_RST);
Ch376msc flashDrive(USB_CS, USB_INT);

Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;
Adafruit_MCP23X17 mcp3;

Preferences prefs;

#if HAS_QR_SOFTWARE_SERIAL
SoftwareSerial QRSerial(QR_RX, QR_TX);
#endif

// =======================================================
// GLOBAL STATE
// =======================================================

uint16_t currentPage = PAGE_LOADING;
uint16_t adminReturnPage = PAGE_LOCKED_SCREEN;
uint16_t currentPageSaved = PAGE_LOADING; // I added

uint16_t adminInputValue = 0;
bool adminInputReceived = false;
String adminInputText = "";
String savedAdminPIN = ADMIN_DEFAULT_PIN_TEXT;

String pendingOldAdminPIN = "";
String pendingNewAdminPIN = "";
String pendingConfirmAdminPIN = "";

String keyboardText = "";
bool keyboardUpperMode = false;
KeyboardTarget keyboardTarget = KB_TARGET_NONE;
uint16_t keyboardReturnPage = PAGE_LOCKED_SCREEN;

CurrentLogEntry currentLogs[MAX_CURRENT_LOGS];
uint8_t currentLogCount = 0;
uint8_t currentLogOffset = 0;
uint8_t activeCurrentLogCards = CURRENT_LOG_CARDS_6;
String currentLogDate = "";
unsigned long lastCurrentLogDateSyncCheck = 0;

TransactionLogEntry transactionLogs[MAX_TRANSACTION_LOGS];
uint8_t transactionLogCount = 0;
uint8_t transactionLogOffset = 0;

String dateLogs[MAX_DATE_LOGS];
uint8_t dateLogCount = 0;
uint8_t dateCardOffset = 0;
int8_t selectedDateVisibleIndex = -1;
String selectedDateText = "";

AccountEntry accounts[MAX_ACCOUNTS];
uint8_t accountCount = 0;
uint8_t accountOffset = 0;

KeyEntry registeredKeys[MAX_KEYS];
uint8_t registeredKeyCount = 0;

bool slotSelected[SLOT_COUNT];
bool slotTaken[SLOT_COUNT];
uint16_t slotDisplayState[SLOT_COUNT];
bool slotMaintenanceActive[SLOT_COUNT];

String pendingRegisterUserName = "";
uint16_t pendingFingerprintID = 0;

String pendingKeyName = "";
String pendingKeyNumber = "";
String pendingKeyRFID = "";

String currentHomeUserName = "";
uint16_t currentHomeFingerprintID = 0;

unsigned long lastRFIDReturnCheck = 0;
String lastReturnedRFIDUID = "";
unsigned long lastRFIDReturnAction = 0;

bool slotUnlockActive[SLOT_COUNT];
unsigned long slotUnlockStartTime[SLOT_COUNT];

bool limitSwitchPreviouslyActive = false;
unsigned long lastLimitSwitchCheck = 0;

bool rtcReady = false;
bool storageSDReady  = false;
bool mcpReady = false;
bool fpsReady = false;
bool rfidReady = false;
bool qrReady = false;
bool usbReady = false;

bool usbInserted = false;
bool previousUSBInserted = false;
unsigned long lastUSBMonitorCheck = 0;

unsigned long lastRTCUpdate = 0;
unsigned long lastFingerprintCheck = 0;
String qrBuffer = "";
unsigned long lastQRByteTime = 0;

bool qrScanActive = false;
int selectedSlotIndex = -1;
uint16_t qrReturnPage = PAGE_HOME_DASHBOARD_A;
String pendingBorrowerName = "";
String pendingStudentCode = "";
String pendingQRRawData = "";

unsigned long lastUserActivityMillis = 0;
bool dwinBacklightSleeping = false;
bool adminAuthenticated = false;

// =======================================================
// FUNCTION DECLARATIONS
// =======================================================

// Debug Serial
void debugLog(String functionName, String message);
void debugValue(String functionName, String label, String value);
void debugNumber(String functionName, String label, long value);
void debugHex16(String functionName, String label, uint16_t value);
void debugOK(String functionName);
void debugFail(String functionName, String reason);

// DWIN
bool dwinRead(uint16_t &receivedVP, uint16_t &receivedValue);
void handleDWIN(uint16_t vp, uint16_t value);
void writeVPWord(uint16_t vp, uint16_t value);
void writeVPText(uint16_t vp, const char *text, uint8_t fieldLength);
void changePage(uint16_t pageID);
void printHex16(uint16_t value);
void setDWINBacklight(uint8_t brightness);
void sleepDWINBacklight();
void wakeDWINBacklight();
void initDisplayIdleTimer();
void registerUserActivity(const char *source);
void handleDisplayIdleSleep();
bool handleDisplayWakeTouch(uint16_t vp, uint16_t value);
void clearTemporarySessionForIdleSleep();

// RTC
bool initRTCModule();
void setRTCFixedOnceIfEnabled();
void updateRTCDisplay();
String getRTCDateString();
String getRTCTimeString();
String getRTCDateTimeString();

// Loading and pages
void runLoadingScreen();
void goToLockedScreen();
void goToHomeDashboard(uint16_t pageID);
void goToQRBorrowerPage();
void renderQRBorrowerPage();
void goToAdminLoginPage();
void renderAdminLoginPage();
void goToAdminDashboard();
void goToAccountPage();
void goToRegistrationPage();
void startNewKeyRegistrationPage();
void goToNewKeyRegistrationPage();
void goToMaintenancePage(uint16_t pageID);
void renderCurrentPage();
bool handlePageCommand(uint16_t vp, uint16_t value);
bool isAdminSourcePage(uint16_t pageID);

// Current logs
void initCurrentLogs();
void renderCurrentLogs(uint8_t visibleCards);
void clearCurrentLogCard(uint8_t cardIndex);
void scrollCurrentLogsUp();
void scrollCurrentLogsDown();
bool loadCurrentLogsForDateFromSD(const String &dateText);
void syncCurrentLogsWithRTCDate();
bool handleCurrentLogControls(uint16_t vp, uint16_t value);
void addCurrentLog(String keyName, String borrowerName, String keyNumber, String timeText, uint16_t statusIcon);

// Transaction logs and date cards
void initTransactionLogs();
void renderTransactionLogs();
void clearTransactionRow(uint8_t rowIndex);
void scrollTransactionLogsUp();
void scrollTransactionLogsDown();
bool handleTransactionLogControls(uint16_t vp, uint16_t value);
void initDateLogs();
bool loadLogsFromSD();
bool ensureArchiveDirectory(const String &path);
void renderDateCards();
String csvEscapeField(const String &field);
bool parseCSVFields(String line, String *fields, uint8_t fieldCount);
bool handleDateCards(uint16_t vp, uint16_t value);
void addTransactionLog(String keyNo, String borrowerName, String keyName, String borrowedTime, String returnedTime, String userName, String dateText);
void addDateLogIfMissing(String dateText);

// Keyboard
bool handleKeyboard(uint16_t vp, uint16_t value);
void openKeyboard(KeyboardTarget target, uint16_t returnPage, bool numberMode);
void openKeyboardPage(uint16_t pageID);
void toggleKeyboardCase();
void keyboardAddChar(char c);
void keyboardBackspace();
void keyboardClear();
void keyboardEnter();
String getKeyboardText();

// Admin
bool handleAdminAccess(uint16_t vp, uint16_t value);
void rememberAdminReturnPage();
void goBackFromAdminLogin();
void goBackFromAdminDashboard();
void handleAdminDownload();
void loadAdminPINFromPreferences();
void saveAdminPINToPreferences(String pinText);
String normalizePINText(String pinText);
String pinValueToFourDigitText(uint16_t value);
bool isValidFourDigitPIN(String pinText);
bool adminPINMatches(String pinText);

// Update Password
void startUpdatePasswordPage();
void goToUpdatePasswordPage();
void renderUpdatePasswordPage();
bool handleUpdatePasswordControls(uint16_t vp, uint16_t value);
void clearUpdatePasswordFields();
void openUpdatePasswordKeyboard(KeyboardTarget target);
void saveUpdatedAdminPassword();

// Account
void loadAccountsFromPreferences();
void renderAccounts();
void clearAccountCard(uint8_t cardIndex);
bool handleAccountControls(uint16_t vp, uint16_t value);
void scrollAccountsUp();
void scrollAccountsDown();
void deleteVisibleAccount(uint8_t visibleIndex);
bool addRegisteredAccount(String userName, uint16_t fingerprintID, String dateRegistered);
void saveAccountsToPreferences();
void compactAccountsInRAM();
bool isFingerprintIDRegistered(uint16_t fingerprintID);
String getAccountNameByFingerprintID(uint16_t fingerprintID);
void setCurrentHomeUserFromFingerprint(uint16_t fingerprintID);
void clearCurrentHomeUser();

// Registration
bool handleRegistrationControls(uint16_t vp, uint16_t value);
void renderRegistrationPage();
void startUserFingerprintRegistration();

// New key registration
bool handleKeyRegistrationControls(uint16_t vp, uint16_t value);
void renderNewKeyRegistrationPage();
void saveNewKeyRegistration();
uint8_t parseSlotNumberFromKeyNumber(String keyNumberText);
String normalizeRFIDUID(String rfidUID);
int findRegisteredKeyIndexBySlot(uint8_t slotNumber);
int findRegisteredKeyIndexByRFID(String rfidUID);
bool upsertRegisteredKey(uint8_t slotNumber, String keyName, String keyNumber, String rfidUID);
bool getRegisteredKeyForSlot(uint8_t slotIndex, KeyEntry &keyOut);
void renderHomeSlotKeyNames();
void clearHomeSlotKeyNames();
void loadKeysFromPreferences();
void saveKeysToPreferences();
void clearRegisteredKeysInRAM();

// Maintenance
void initMaintenanceStateDefaults();
void loadMaintenanceState();
void saveMaintenanceState();
void saveSlotTakenState(uint8_t slotIndex);
void renderMaintenanceSlots();
bool handleMaintenanceControls(uint16_t vp, uint16_t value);
bool handleSlotControls(uint16_t vp, uint16_t value);
void setSelectedSlotsActive(bool active);
void updateSlotDisplay(uint8_t slotIndex);
void clearSlotSelections();

// Hardware
void initHardwarePins();
bool initIOExpanders();
void setSlotLockOutput(uint8_t slotIndex, bool unlocked);
void initLimitSwitch();
void handleLimitSwitchSafetyLock();
void lockAllSlotSolenoids();
bool initStorageModules();
bool initUSBHost();
bool usbHostConnected();
void monitorUSBInsertion();
void notifyUSBInserted();
void notifyUSBRemoved();

// Fingerprint
bool initFingerprintModule();
void checkLockedScreenFingerprint();
int getFingerprintID();
uint16_t findFreeFingerprintID();
bool enrollFingerprintBlocking(uint16_t id);
bool verifyStoredFingerprintID(uint16_t id);
int waitForFingerImage(uint32_t timeoutMs);
bool waitForFingerRelease(uint32_t timeoutMs);

// RFID
bool initRFIDModule();
String readRFIDTagOnce();
String waitForRFIDTag(uint32_t timeoutMs);
void checkLockedScreenRFIDReturn();
void markRegisteredKeyReturned(uint8_t keyIndex);
void updateReturnUnlockRelock();
void startTimedSlotUnlock(uint8_t slotIndex);

// QR / Borrower Identification
bool initQRScanner();
void handleQRScanner();
bool handleQRControls(uint16_t vp, uint16_t value);
void startQRScanForBorrower();
void processQRData(String qrData);
String normalizeQRData(String qrData);
String getFirstQRToken(String qrData);
String getLastQRToken(String qrData);
String removeFirstQRToken(String qrData);
String removeLastQRToken(String qrData);
bool isNumericQRToken(String token);
bool isStudentFormatQR(String qrData);
bool isCourseOrSectionQRToken(String token);
String extractBorrowerCodeFromQR(String qrData);
String extractBorrowerNameFromQR(String qrData);
String findStudentNameFromCode(String studentCode);
void confirmBorrowerIdentity();
void cancelQRBorrowerPage();

// Project reset
void resetEverythingForActualProject();

// Storage / USB
bool writeTextFileToDaySD(String path, String content);
bool appendLineToDaySD(String path, String line);
void downloadSelectedDateLogToUSB();

// Compile the feature tabs as part of this translation unit so they share
// the configuration, globals, and declarations above.
#include "00_Debug.h"
#include "01_DWIN.h"
#include "02_RTC.h"
#include "03_Loading.h"
#include "04_Pages.h"
#include "05_CurrentLogs.h"
#include "06_TransactionLogs.h"
#include "07_Keyboard.h"
#include "08_Admin.h"
#include "09_Account.h"
#include "10_Registration.h"
#include "11_KeyRegistration.h"
#include "12_Maintenance.h"
#include "13_Hardware.h"
#include "14_Fingerprint.h"
#include "15_RFID.h"
#include "16_QR.h"
#include "17_StorageUSB.h"
#include "18_ProjectReset.h"
#include "19_LimitSwitch.h"
#include "20_DisplayIdle.h"
#include "21_UpdatePassword.h"

// =======================================================
// SETUP + LOOP
// =======================================================

void setup() {
  Serial.begin(115200);

  initHardwarePins();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(100);
  initIOExpanders();

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  
  Serial.println();
  Serial.println("DWIN Smart Locker Full Modular Final Integrated Update Password Code Starting...");

  DWIN.begin(DWIN_BAUD, SERIAL_8N1, DWIN_RX, DWIN_TX);

  initLimitSwitch();
  initMaintenanceStateDefaults();

  runLoadingScreen();
  initDisplayIdleTimer();

  //resetEverythingForActualProject();  // uncomment this line to reset everything for the actual project, including clearing all data and settings.
}

void loop() {
  uint16_t vp;
  uint16_t value;

  if (dwinRead(vp, value)) {
    handleDWIN(vp, value);
  }

  handleLimitSwitchSafetyLock();

  updateRTCDisplay();
  syncCurrentLogsWithRTCDate();
  monitorUSBInsertion();
  handleDisplayIdleSleep();

  if (currentPage == PAGE_LOCKED_SCREEN ||
      currentPage == PAGE_HOME_DASHBOARD_A ||
      currentPage == PAGE_HOME_DASHBOARD_B) {
    if (currentPage == PAGE_LOCKED_SCREEN) {
      checkLockedScreenFingerprint();
    }
    if (currentPage == PAGE_HOME_DASHBOARD_A || currentPage == PAGE_HOME_DASHBOARD_B) {
      checkLockedScreenRFIDReturn();
    }
  }

  updateReturnUnlockRelock();

  if (currentPage == PAGE_QR_BORROWER && qrScanActive) {
    handleQRScanner();
  }
}
