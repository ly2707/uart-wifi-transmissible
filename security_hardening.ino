// ==================== Security Hardening ====================

SecurityState usbSecurityState = {0, 0, 0};
SecurityState webSecurityState = {0, 0, 0};
SecurityState clientTcpSecurityState = {0, 0, 0};
SecurityState serverTcpSecurityStates[MAX_CLIENTS];

String usbFrameBuffer = "";
String webFrameBuffer = "";
String clientTcpFrameBuffer = "";
String serverTcpFrameBuffers[MAX_CLIENTS];

String buildSecureFrame(const String &payload) {
  String errorReason;
  if (!validateTransparentPayload(payload, errorReason)) {
    return "";
  }

  return String(SECURITY_FRAME_HEADER_CHAR) + String(payload.length()) + String(SECURITY_FRAME_SEPARATOR_CHAR) + payload + String(SECURITY_FRAME_TAIL_CHAR);
}

bool sendValidatedPayloadToUART(uart_port_t uartPort, const String &payload) {
  String errorReason;
  if (!validateTransparentPayload(payload, errorReason)) {
    return false;
  }

  int written = uart_write_bytes(uartPort, payload.c_str(), payload.length());
  return written == (int)payload.length();
}

bool sendFramedPayloadToClient(WiFiClient &client, const String &payload) {
  String frame = buildSecureFrame(payload);
  if (frame.length() == 0) {
    return false;
  }

  size_t written = client.write((const uint8_t *)frame.c_str(), frame.length());
  return written == frame.length();
}

bool isAsciiDigitChar(char value) {
  return value >= '0' && value <= '9';
}

bool isSourceBlocked(SecurityState &state) {
  return state.blockUntil != 0 && millis() < state.blockUntil;
}

void clearSecurityState(SecurityState &state) {
  state.failureCount = 0;
  state.blockUntil = 0;
  state.lastActivity = millis();
}

void recordSecurityActivity(SecurityState &state) {
  state.lastActivity = millis();
}

void recordSecurityFailure(SecurityState &state) {
  if (state.failureCount < 255) {
    state.failureCount++;
  }
  state.lastActivity = millis();
  if (state.failureCount >= SECURITY_MAX_INVALID_ATTEMPTS) {
    state.blockUntil = millis() + SECURITY_LOCKOUT_MS;
  }
}

void recordSecuritySuccess(SecurityState &state) {
  state.failureCount = 0;
  state.blockUntil = 0;
  state.lastActivity = millis();
}

bool isAllowedFrameCharacter(char value) {
  if (rawTransmitMode) {
    return value != '\0';
  }

  if (value == '\r' || value == '\n' || value == '\t') {
    return true;
  }

  return value >= 0x20 && value <= 0x7E;
}

bool isAllowedPayloadCharacter(char value) {
  if (rawTransmitMode) {
    return value != '\0';
  }

  if (value == '\r' || value == '\n' || value == '\t') {
    return true;
  }

  return value >= 0x20 && value <= 0x7E && value != SECURITY_FRAME_HEADER_CHAR && value != SECURITY_FRAME_TAIL_CHAR;
}

bool isTelemetryLikePayload(const String &payload) {
  if (payload.length() == 0) {
    return false;
  }

  char firstChar = payload[0];
  bool validLead = (firstChar >= '0' && firstChar <= '9') ||
                   (firstChar >= 'A' && firstChar <= 'Z') ||
                   (firstChar >= 'a' && firstChar <= 'z') ||
                   firstChar == '$' || firstChar == '!' || firstChar == '%';
  if (!validLead) {
    return false;
  }

  bool hasAlphaNumeric = false;
  for (unsigned int i = 0; i < payload.length(); i++) {
    char value = payload[i];
    if ((value >= '0' && value <= '9') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z')) {
      hasAlphaNumeric = true;
      continue;
    }

    if (value == '\r' || value == '\n' || value == '\t' || value == ' ' ||
        value == '.' || value == ',' || value == ';' || value == ':' ||
        value == '_' || value == '-' || value == '=' || value == '+' ||
        value == '/' || value == '|' || value == '(' || value == ')' ||
        value == '[' || value == ']' || value == '{' || value == '}' ||
        value == '?' || value == '&' || value == '%') {
      continue;
    }

    return false;
  }

  return hasAlphaNumeric;
}

bool payloadMatchesWhitelist(const String &payload) {
  String normalized = payload;
  normalized.trim();
  normalized.toUpperCase();

  if (normalized.length() == 0) {
    return false;
  }

  static const char *allowedPrefixes[] = {
    "AT+",
    "CLIENT_ID:",
    "SERVER:",
    "CMD:",
    "DATA:",
    "REQ:",
    "RESP:",
    "STATUS",
    "PING",
    "PONG",
    "READ",
    "WRITE",
    "SET",
    "GET",
    "INFO",
    "QUERY",
    "MEAS",
    "$",
    "!"
  };

  static const char *allowedExactTokens[] = {
    "OK",
    "ACK",
    "NACK",
    "ON",
    "OFF",
    "TRUE",
    "FALSE",
    "0",
    "1"
  };

  for (size_t i = 0; i < sizeof(allowedExactTokens) / sizeof(allowedExactTokens[0]); i++) {
    if (normalized == allowedExactTokens[i]) {
      return true;
    }
  }

  for (size_t i = 0; i < sizeof(allowedPrefixes) / sizeof(allowedPrefixes[0]); i++) {
    if (normalized.startsWith(allowedPrefixes[i])) {
      return true;
    }
  }

  return isTelemetryLikePayload(payload);
}

bool payloadContainsBlockedToken(const String &payload) {
  String normalized = payload;
  normalized.toUpperCase();

  static const char *blockedTokens[] = {
    "AT+RST",
    "AT+RESET",
    "AT+RESTART",
    "AT+RESTORE",
    "AT+CONFIG",
    "AT+EXITCONFIG",
    "AT+SAVETRANSLINK",
    "AT+CIPSTART",
    "AT+CIPSERVER",
    "AT+CWMODE",
    "+++",
    "../",
    "..\\",
    "<SCRIPT"
  };

  for (size_t i = 0; i < sizeof(blockedTokens) / sizeof(blockedTokens[0]); i++) {
    if (normalized.indexOf(blockedTokens[i]) >= 0) {
      return true;
    }
  }

  return false;
}

bool validateTransparentPayload(const String &payload, String &errorReason) {
  if (payload.length() == 0) {
    errorReason = "empty payload";
    return false;
  }

  if (payload.length() > SECURITY_MAX_TRANSPARENT_PAYLOAD) {
    errorReason = "payload too large";
    return false;
  }

  for (unsigned int i = 0; i < payload.length(); i++) {
    if (!isAllowedPayloadCharacter(payload[i])) {
      errorReason = "payload contains illegal character";
      return false;
    }
  }

  if (rawTransmitMode) {
    return true;
  }

  if (payloadContainsBlockedToken(payload)) {
    errorReason = "payload contains blocked token";
    return false;
  }

  if (!payloadMatchesWhitelist(payload)) {
    errorReason = "payload not in whitelist";
    return false;
  }

  return true;
}

bool parseSecureFrame(const String &frame, String &payload, String &errorReason) {
  if (frame.length() < 5 || frame.length() > SECURITY_MAX_FRAME_LENGTH) {
    errorReason = "frame length invalid";
    return false;
  }

  if (frame[0] != SECURITY_FRAME_HEADER_CHAR || frame[frame.length() - 1] != SECURITY_FRAME_TAIL_CHAR) {
    errorReason = "frame boundary invalid";
    return false;
  }

  int separatorPos = frame.indexOf(SECURITY_FRAME_SEPARATOR_CHAR);
  if (separatorPos <= 1) {
    errorReason = "frame separator missing";
    return false;
  }

  String lengthToken = frame.substring(1, separatorPos);
  for (unsigned int i = 0; i < lengthToken.length(); i++) {
    if (!isAsciiDigitChar(lengthToken[i])) {
      errorReason = "frame length field invalid";
      return false;
    }
  }

  int expectedLength = lengthToken.toInt();
  if (expectedLength <= 0 || expectedLength > SECURITY_MAX_TRANSPARENT_PAYLOAD) {
    errorReason = "frame payload length out of range";
    return false;
  }

  payload = frame.substring(separatorPos + 1, frame.length() - 1);
  if (payload.length() != expectedLength) {
    errorReason = "frame payload length mismatch";
    return false;
  }

  return validateTransparentPayload(payload, errorReason);
}

String *getSecurityFrameBuffer(SecurityInputSource source, int clientIndex) {
  switch (source) {
    case SECURITY_SOURCE_USB:
      return &usbFrameBuffer;
    case SECURITY_SOURCE_WEB:
      return &webFrameBuffer;
    case SECURITY_SOURCE_TCP_CLIENT:
      return &clientTcpFrameBuffer;
    case SECURITY_SOURCE_TCP_SERVER:
      if (clientIndex >= 0 && clientIndex < MAX_CLIENTS) {
        return &serverTcpFrameBuffers[clientIndex];
      }
      return NULL;
    default:
      return NULL;
  }
}

SecurityState *getSecurityState(SecurityInputSource source, int clientIndex) {
  switch (source) {
    case SECURITY_SOURCE_USB:
      return &usbSecurityState;
    case SECURITY_SOURCE_WEB:
      return &webSecurityState;
    case SECURITY_SOURCE_TCP_CLIENT:
      return &clientTcpSecurityState;
    case SECURITY_SOURCE_TCP_SERVER:
      if (clientIndex >= 0 && clientIndex < MAX_CLIENTS) {
        return &serverTcpSecurityStates[clientIndex];
      }
      return NULL;
    default:
      return NULL;
  }
}

bool enforceSecurityTimeout(SecurityInputSource source, int clientIndex) {
  String *buffer = getSecurityFrameBuffer(source, clientIndex);
  SecurityState *state = getSecurityState(source, clientIndex);
  if (buffer == NULL || state == NULL) {
    return false;
  }

  if (buffer->length() > 0 && state->lastActivity != 0 && (millis() - state->lastActivity) > SECURITY_INPUT_TIMEOUT_MS) {
    *buffer = "";
    recordSecurityFailure(*state);
    return true;
  }

  if (state->blockUntil != 0 && millis() >= state->blockUntil) {
    clearSecurityState(*state);
  }

  return false;
}

void refreshSecurityTimeouts() {
  enforceSecurityTimeout(SECURITY_SOURCE_USB, -1);
  enforceSecurityTimeout(SECURITY_SOURCE_WEB, -1);
  enforceSecurityTimeout(SECURITY_SOURCE_TCP_CLIENT, -1);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    enforceSecurityTimeout(SECURITY_SOURCE_TCP_SERVER, i);
  }
}

bool appendIngressChunk(SecurityInputSource source, int clientIndex, const uint8_t *data, size_t len, String &errorReason) {
  String *buffer = getSecurityFrameBuffer(source, clientIndex);
  SecurityState *state = getSecurityState(source, clientIndex);
  if (buffer == NULL || state == NULL || data == NULL) {
    errorReason = "security context unavailable";
    return false;
  }

  enforceSecurityTimeout(source, clientIndex);
  if (isSourceBlocked(*state)) {
    errorReason = "security source blocked";
    return false;
  }

  if (len == 0 || len > SECURITY_MAX_FRAME_LENGTH) {
    recordSecurityFailure(*state);
    errorReason = "security chunk length invalid";
    return false;
  }

  if (buffer->length() + len > SECURITY_MAX_FRAME_LENGTH) {
    *buffer = "";
    recordSecurityFailure(*state);
    errorReason = "security frame buffer overflow";
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    char value = (char)data[i];
    if (!isAllowedFrameCharacter(value)) {
      *buffer = "";
      recordSecurityFailure(*state);
      errorReason = "security frame contains illegal byte";
      return false;
    }
    *buffer += value;
  }

  int headerPos = buffer->indexOf(SECURITY_FRAME_HEADER_CHAR);
  if (headerPos < 0) {
    *buffer = "";
    recordSecurityFailure(*state);
    errorReason = "security frame header missing";
    return false;
  }

  if (headerPos > 0) {
    buffer->remove(0, headerPos);
  }

  recordSecurityActivity(*state);
  return true;
}

bool getValidatedPayload(SecurityInputSource source, int clientIndex, String &payload, String &errorReason) {
  String *buffer = getSecurityFrameBuffer(source, clientIndex);
  SecurityState *state = getSecurityState(source, clientIndex);
  if (buffer == NULL || state == NULL) {
    errorReason = "security context unavailable";
    return false;
  }

  enforceSecurityTimeout(source, clientIndex);
  if (isSourceBlocked(*state) || buffer->length() == 0) {
    return false;
  }

  int headerPos = buffer->indexOf(SECURITY_FRAME_HEADER_CHAR);
  if (headerPos < 0) {
    *buffer = "";
    recordSecurityFailure(*state);
    errorReason = "security frame header missing";
    return false;
  }
  if (headerPos > 0) {
    buffer->remove(0, headerPos);
  }

  int separatorPos = buffer->indexOf(SECURITY_FRAME_SEPARATOR_CHAR, 1);
  if (separatorPos <= 1) {
    return false;
  }

  String lengthToken = buffer->substring(1, separatorPos);
  for (unsigned int i = 0; i < lengthToken.length(); i++) {
    if (!isAsciiDigitChar(lengthToken[i])) {
      *buffer = "";
      recordSecurityFailure(*state);
      errorReason = "frame length field invalid";
      return false;
    }
  }

  int expectedLength = lengthToken.toInt();
  if (expectedLength <= 0 || expectedLength > SECURITY_MAX_TRANSPARENT_PAYLOAD) {
    *buffer = "";
    recordSecurityFailure(*state);
    errorReason = "frame payload length out of range";
    return false;
  }

  int tailPos = separatorPos + expectedLength + 1;
  if (buffer->length() <= tailPos) {
    return false;
  }

  if ((*buffer)[tailPos] != SECURITY_FRAME_TAIL_CHAR) {
    *buffer = "";
    recordSecurityFailure(*state);
    errorReason = "frame boundary invalid";
    return false;
  }

  String frame = buffer->substring(0, tailPos + 1);
  buffer->remove(0, tailPos + 1);

  if (!parseSecureFrame(frame, payload, errorReason)) {
    recordSecurityFailure(*state);
    return false;
  }

  recordSecuritySuccess(*state);
  return true;
}

bool isSecuritySourceBlocked(SecurityInputSource source, int clientIndex) {
  SecurityState *state = getSecurityState(source, clientIndex);
  if (state == NULL) {
    return true;
  }

  enforceSecurityTimeout(source, clientIndex);
  return isSourceBlocked(*state);
}

uint8_t getSecurityFailureCount(SecurityInputSource source, int clientIndex) {
  SecurityState *state = getSecurityState(source, clientIndex);
  if (state == NULL) {
    return SECURITY_MAX_INVALID_ATTEMPTS;
  }
  return state->failureCount;
}
