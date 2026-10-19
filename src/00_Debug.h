// =======================================================
// Debug.ino
// Serial Monitor helper functions.
// =======================================================

/*
  Function: debugLog()
  Description:
    Prints a function-tagged message to Serial Monitor.

  How to use:
    debugLog("functionName", "message here");
*/
void debugLog(String functionName, String message) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.print("] ");
  Serial.println(message);
#endif
}

/*
  Function: debugValue()
  Description:
    Prints a function-tagged text label and text value.

  How to use:
    debugValue("functionName", "label", valueString);
*/
void debugValue(String functionName, String label, String value) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.print("] ");
  Serial.print(label);
  Serial.print(": ");
  Serial.println(value);
#endif
}

/*
  Function: debugNumber()
  Description:
    Prints a function-tagged text label and decimal number.

  How to use:
    debugNumber("functionName", "count", 5);
*/
void debugNumber(String functionName, String label, long value) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.print("] ");
  Serial.print(label);
  Serial.print(": ");
  Serial.println(value);
#endif
}

/*
  Function: debugHex16()
  Description:
    Prints a function-tagged text label and 16-bit hexadecimal value.

  How to use:
    debugHex16("handleDWIN", "VP", vp);
*/
void debugHex16(String functionName, String label, uint16_t value) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.print("] ");
  Serial.print(label);
  Serial.print(": 0x");
  if (value < 0x1000) Serial.print("0");
  if (value < 0x0100) Serial.print("0");
  if (value < 0x0010) Serial.print("0");
  Serial.println(value, HEX);
#endif
}

/*
  Function: debugOK()
  Description:
    Prints that a function completed successfully.

  How to use:
    debugOK("initRTCModule");
*/
void debugOK(String functionName) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.println("] OK");
#endif
}

/*
  Function: debugFail()
  Description:
    Prints that a function failed and why.

  How to use:
    debugFail("initRTCModule", "RTC not found");
*/
void debugFail(String functionName, String reason) {
#if DEBUG_SERIAL
  Serial.print("[");
  Serial.print(functionName);
  Serial.print("] FAILED: ");
  Serial.println(reason);
#endif
}
