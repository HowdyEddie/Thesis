// =======================================================
// RTC.ino
// DS3231 RTC functions. Date and time are shown on every page.
// =======================================================

/*
  Function: initRTCModule()
  Description:
    Starts the DS3231 RTC module.

  How to use:
    Called during loading screen.
*/
bool initRTCModule() {
  rtcReady = rtc.begin();

  if (!rtcReady) {
    debugFail("initRTCModule", "RTC not found. Check SDA/SCL/power/address.");
    return false;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power. Setting RTC to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  setRTCFixedOnceIfEnabled();

  debugOK("initRTCModule");
  return true;
}

/*
  Function: setRTCFixedOnceIfEnabled()
  Description:
    Sets the DS3231 RTC to a fixed manual date/time when SET_RTC_FIXED_ONCE is 1.

  How to use:
    Set SET_RTC_FIXED_ONCE to 1, upload once, then change it back to 0 and upload again.
*/
void setRTCFixedOnceIfEnabled() {
#if SET_RTC_FIXED_ONCE
  rtc.adjust(DateTime(
    RTC_FIXED_YEAR,
    RTC_FIXED_MONTH,
    RTC_FIXED_DAY,
    RTC_FIXED_HOUR,
    RTC_FIXED_MINUTE,
    RTC_FIXED_SECOND
  ));

  Serial.println("RTC manually set to fixed date/time.");
#endif
}

/*
  Function: updateRTCDisplay()
  Description:
    Updates the global RTC date/time VPs once per second.

  How to use:
    Called continuously in loop().
    Put Text Display objects with VP 0x1500 and 0x1510 on every DWIN page.
*/
void updateRTCDisplay() {
  if (!rtcReady) return;

  if (millis() - lastRTCUpdate < 1000) return;
  lastRTCUpdate = millis();

  writeVPText(VP_RTC_DATE_DISPLAY, getRTCDateString().c_str(), RTC_DATE_BYTES);
  writeVPText(VP_RTC_TIME_DISPLAY, getRTCTimeString().c_str(), RTC_TIME_BYTES);
}

/*
  Function: getRTCDateString()
  Description:
    Returns RTC date as YYYY-MM-DD.

  How to use:
    String today = getRTCDateString();
*/
String getRTCDateString() {
  if (!rtcReady) return "0000-00-00";

  DateTime now = rtc.now();
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", now.year(), now.month(), now.day());
  return String(buffer);
}

/*
  Function: getRTCTimeString()
  Description:
    Returns RTC time as HH:MM:SS.

  How to use:
    String timeNow = getRTCTimeString();
*/
String getRTCTimeString() {
  if (!rtcReady) return "00:00:00";

  DateTime now = rtc.now();
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(buffer);
}

/*
  Function: getRTCDateTimeString()
  Description:
    Returns RTC date and time in one string.

  How to use:
    String stamp = getRTCDateTimeString();
*/
String getRTCDateTimeString() {
  return getRTCDateString() + " " + getRTCTimeString();
}
