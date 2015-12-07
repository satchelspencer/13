#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <ESP8266TrueRandom.h>
#include <Hash.h>
#include <WebSocketsServer.h>

const char WifiAPSSID[] = "3308-13";
IPAddress WifiAPIP(192, 168, 1, 1);
const char WifiDOMAIN[] = "192.168.1.1";
const char ServerURL[] = "http://192.168.1.1/";
const int MAX_SESSIONS = 4;
int sessionCount = 0;
String sessions[MAX_SESSIONS][3];
const String COOKIE_KEY = "SessionID=";
ESP8266WebServer server(80);
WebSocketsServer wsServer(81);

/*
Filesystem functions
*/

bool fileExists(String path) {
  return SPIFFS.exists(path);
}

File openFileR(String path) {
  return SPIFFS.open(path, "r");
}

bool initializeFilesystem() {
  return SPIFFS.begin();
}

/*
Session functions
*/

String randomSessionID() {
  char bytes[16];
  ESP8266TrueRandom.memfill(bytes, 16);
  String hexString = "";
  for (int i = 0; i < 16; i++) {
    hexString += String(bytes[i], HEX);
  }
  return hexString;
}

int addSession() {
  if (sessionCount >= MAX_SESSIONS) return -1;
  sessions[sessionCount][0] = randomSessionID();
  sessions[sessionCount][1] = server.arg("username");
  sessions[sessionCount][2] = -1;
  return sessionCount++;
}

void removeSession(int sessionIndex) {
  if (sessionIndex >= sessionCount) return;
  for (int i = sessionIndex; i < sessionCount - 1; i++) {
    sessions[i][0] = sessions[i + 1][0];
    sessions[i][1] = sessions[i + 1][1];
    sessions[i][2] = sessions[i + 1][2];
  }
  sessionCount--;
}

int sessionIndexFromID(String sessionID) {
  for (int i = 0; i < sessionCount; i++) {
    if (sessions[i][0] == sessionID) return i;
  }
  return -1;
}

int sessionIndexFromWebsocket(int num) {
  for (int i = 0; i < sessionCount; i++) {
    if (sessions[i][2].toInt() == num) return i;
  }
  return -1;
}

int sessionIndexFromCookie() {
  int sessionIndex = -1;
  if (server.hasHeader("Cookie")) {
    String cookieValue = server.header("Cookie");
    int sessionStart = cookieValue.indexOf(COOKIE_KEY) + COOKIE_KEY.length();
    if (sessionStart != -1) {
      int sessionEnd = cookieValue.indexOf(";", sessionStart);
      if (sessionEnd == -1) sessionIndex = sessionIndexFromID(cookieValue.substring(sessionStart));
      else sessionIndex = sessionIndexFromID(cookieValue.substring(sessionStart, sessionEnd));
    }
  }
  return sessionIndex;
}

void sendSessionCookie(int sessionIndex, bool expire) {
  String sessionCookie = COOKIE_KEY;
  if (!expire) sessionCookie += sessions[sessionIndex][0];
  sessionCookie += "; Domain=";
  sessionCookie += WifiDOMAIN;
  sessionCookie += "; Path=/";
  if (expire) sessionCookie += "; expires=Thu, 01 Jan 1970 00:00:00 UTC";
  server.sendHeader("Set-Cookie", sessionCookie);
}

/*
Login/out functions
*/

bool authenticateUser(String username, String password) {
  File f = openFileR("/db_users.csv");
  if (f) {
    char lf = 10;
    f.readStringUntil(lf);
    while (f.available()) {
      char *line = strdup(f.readStringUntil(lf).c_str());
      char *p;
      p = strtok(line, ",");
      if (String(p) == username) {
        p = strtok(NULL, ",");
        String hash = sha1(username + password + p);
        p = strtok(NULL, ",");
        if (String(p) == hash) {
          free(line);
          return true;
        }
      }
      free(line);
    }
    f.close();
  }
  return false;
}

String handleLogin() {
  bool loginSuccessful = authenticateUser(server.arg("username"), server.arg("password"));
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(2);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["success"] = false;
  if (loginSuccessful) {
    int sessionIndex = addSession();
    if (sessionIndex != -1) {
      root["success"] = true;
      sendSessionCookie(sessionIndex, false);
      return jsonEncode(root, 17);
    } else root["error"] = 2;
  } else root["error"] = 1;
  // {"success":false,"error":2}\0 = 28 chars
  // error 1: bad credentials, error 2: no available sessions
  return jsonEncode(root, 28);
}

String handleLogout(int sessionIndex) {
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(1);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["success"] = false;
  if (sessionIndex != -1) {
    removeSession(sessionIndex);
    root["success"] = true;
    sendSessionCookie(-1, true);
    return jsonEncode(root, 17);
  }
  // {"success":false}\0 = 18 chars
  return jsonEncode(root, 18);
}

/*
Utility functions
*/

String getContentType(String path) {
  if (path.endsWith(".html")) return "text/html";
  else if (path.endsWith(".js")) return "application/javascript";
  else if (path.endsWith(".css")) return "text/css";
  return "text/plain";
}

String jsonEncode(JsonObject& root, int maxOutputLength) {
  char output[maxOutputLength];
  root.printTo(output, maxOutputLength);
  return String(output);
}

void sendRedirect(int code, String path) {
  server.sendHeader("Location", ServerURL + path, true);
  server.send(code);
}

/*
HTTP request handler functions
*/

String handleWhoAmI(int sessionIndex) {
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(2);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["success"] = true;
  root["username"] = (sessions[sessionIndex][1]).c_str();
  // {"success":true,"username":"12345678"}\0 = 39 chars
  return jsonEncode(root, 39);
}

bool handleAPIRequest(String path, int sessionIndex) {
  if (!(path.startsWith("/api/"))) return false;
  path = path.substring(5);
  String returnJSON;
  if (path == "login") returnJSON = handleLogin();
  else if (path == "logout") returnJSON = handleLogout(sessionIndex);
  else if (path == "whoami") returnJSON = handleWhoAmI(sessionIndex);
  else return false;
  server.send(200, "application/json", returnJSON);
  return true;
}

bool handleRedirects(String path, int sessionIndex) {
  if (sessionIndex == -1 && !path.endsWith(".js") && !path.endsWith(".css") && path != "/login.html" && path != "/api/login") {
    sendRedirect(307, "login.html");
  } else return false;
  return true;
}

bool handleRequest(String path) {
  int sessionIndex = sessionIndexFromCookie();
  if (handleRedirects(path, sessionIndex)) return true;
  if (handleAPIRequest(path, sessionIndex)) return true;
  bool gz = false;
  if (!fileExists(path)) {
    path += ".gz";
    if (!fileExists(path)) {
      return false;
    } else gz = true;
  }
  File f = openFileR(path);
  if (f) {
    if (gz) path = path.substring(0, path.length() - 3);
    String contentType = getContentType(path);
    server.streamFile(f, contentType);
    f.close();
    return true;
  }
  return false;
}

/*
WebSocket functions
*/

void wsHandleProtocolChat(String command, String body, uint8_t num) {
  if (command == "message") {
    for (int i = 0; i < sessionCount; i++) {
      int sendToNum = sessions[i][2].toInt();
      if (sendToNum != -1) {
        wsServer.sendTXT(sendToNum, "chat:message:" + sessions[i][1] + ";" + body);
      }
    }
  }
}

void wsHandleProtocolGlobal(String command, String body, uint8_t num) {
  if (command == "connect") {
    int sessionIndex = sessionIndexFromID(body);
    if (sessionIndex == -1) {
      wsServer.sendTXT(num, "global:connect:fail");
    } else {
      sessions[sessionIndex][2] = String(num);
      wsServer.sendTXT(num, "global:connect:success");
    }
  } else {
    wsServer.sendTXT(num, "global:error:pleaseAuthenticate");
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Connected to num: " + String(num));
      break;
    case WStype_DISCONNECTED: {
        int sessionIndex = sessionIndexFromWebsocket(num);
        if (sessionIndex != -1) sessions[sessionIndex][2] = String(-1);
        Serial.println("Disconnected from num: " + String(num));
        break;
      }
    case WStype_TEXT:
      String payloadString = String((char *)payload);
      String data[3];
      int startIndex = 0;
      int index = payloadString.indexOf(":");
      int pieces = 0;
      int count = 0;
      while (index != -1) {
        String piece = payloadString.substring(startIndex, index);
        data[pieces++] = piece;
        startIndex = index + 1;
        if (pieces == 2) {
          index++;
          break;
        }
        index = payloadString.indexOf(":", startIndex);
      }
      data[2] = payloadString.substring(index, payloadString.length());

      String protocol = data[0];
      String command = data[1];
      String body = data[2];

      bool authenticated = (sessionIndexFromWebsocket(num) != -1);
      if (authenticated) {
        if (protocol == "chat") wsHandleProtocolChat(command, body, num);
      } else {
        if (protocol == "global") wsHandleProtocolGlobal(command, body, num);
      }
      break;
  }
}

/*
Setup and loop functions
*/

void setup() {
  // Setup Serial communication
  Serial.begin(115200);
  Serial.println();

  // Setup WiFi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(WifiAPIP, WifiAPIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WifiAPSSID);

  // Serve a 404 if the filesystem can't be initialized
  if (!initializeFilesystem()) {
    server.onNotFound([]() {
      server.send(404, "text/plain", "404: Failed to initialize file system");
    });
    return;
  }

  // Serve a 404 if handleRequest() can't handle the request
  server.onNotFound([]() {
    if (!handleRequest(server.uri())) server.send(404, "text/plain", "404: File not found");
  });

  // Ask the server to collect Cookie headers
  const size_t headerKeysCount = 1;
  const char* headerKeys[headerKeysCount] = {"Cookie"};
  server.collectHeaders(headerKeys, headerKeysCount);

  // Start the server
  server.begin();
  wsServer.begin();
  wsServer.onEvent(webSocketEvent);
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
  wsServer.loop();
}
