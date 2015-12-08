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

/**
 * check if a given path exists in the file system
 * @param path the path to check
 * @return true if the file exists, false if the file does not exist
 */

bool fileExists(String path) {
  return SPIFFS.exists(path);
}

/**
 * open a file from the file system given a path
 * @param path the path to the file
 * @return the opened File
 */

File openFileR(String path) {
  return SPIFFS.open(path, "r");
}

/**
 * initialize the file system
 * @return true if initialized successfully, false if unsuccessful
 */

bool initializeFilesystem() {
  return SPIFFS.begin();
}

/*
Session functions
*/

/**
 * generate a random 16 byte hex string for use as a session ID
 * @return the random 16 byte hex string
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

/**
 * adds a new session to the session data structure and increments sessionCount
 * @return the index of the session that was added
 */

int addSession() {
  if (sessionCount >= MAX_SESSIONS) return -1;
  sessions[sessionCount][0] = randomSessionID();
  sessions[sessionCount][1] = server.arg("username");
  sessions[sessionCount][2] = -1;
  return sessionCount++;
}

/**
 * removes a session from the session data structure, ensures the remaining sessions are contiguous in memory, and decrements sessionCount
 * @param sessionIndex the session to remove
 */

void removeSession(int sessionIndex) {
  if (sessionIndex >= sessionCount) return;
  for (int i = sessionIndex; i < sessionCount - 1; i++) {
    sessions[i][0] = sessions[i + 1][0];
    sessions[i][1] = sessions[i + 1][1];
    sessions[i][2] = sessions[i + 1][2];
  }
  sessionCount--;
}

/**
 * get a session index given a session ID
 * @param sessionID the session ID to look for in current sessions
 * @return the session index for the session ID
 */

int sessionIndexFromID(String sessionID) {
  for (int i = 0; i < sessionCount; i++) {
    if (sessions[i][0] == sessionID) return i;
  }
  return -1;
}

/**
 * get a session index given a websocket num
 * @param num the websocket num to look for in current sessions
 * @return the session index for the websocket num
 */

int sessionIndexFromWebsocket(int num) {
  for (int i = 0; i < sessionCount; i++) {
    if (sessions[i][2].toInt() == num) return i;
  }
  return -1;
}

/**
 * gets the current session index using the cookie header
 * @return the current session index
 */

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

/**
 * tells a client to set or remove its session cookie
 * @param sessionIndex the session index of the client
 * @param expire whether to expire the cookie or not
 */

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

/**
 * authenticates a user using the user database given a username and password
 * @param username the user's username
 * @param password the user's password
 * @return true if authentication is successful, false otherwise
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

/**
 * handles the API for login attempts
 * @return a JSON string responding to the login attempt
 */

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

/**
 * handles the API for logging out
 * @param sessionIndex the session index of the client to log out
 * @return a JSON string responding to the logout attempt
 */

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

/**
 * get a content type given a file path
 * @param path the path of the file to check
 * @return the content type of a file
 */

String getContentType(String path) {
  if (path.endsWith(".html")) return "text/html";
  else if (path.endsWith(".js")) return "application/javascript";
  else if (path.endsWith(".css")) return "text/css";
  return "text/plain";
}

/**
 * encode a JsonObject to a string
 * @param root the JSON object to encode
 * @param maxOutputLength the maximum length of the encoded string (include null termination char)
 * @return a JSON string containing the provided object
 */

String jsonEncode(JsonObject& root, int maxOutputLength) {
  char output[maxOutputLength];
  root.printTo(output, maxOutputLength);
  return String(output);
}

/**
 * sends a redirect header and status code to the client
 * @param code the status code to send
 * @param path the URL path to redirect to
 */

void sendRedirect(int code, String path) {
  server.sendHeader("Location", ServerURL + path, true);
  server.send(code);
}

/*
HTTP request handler functions
*/

/**
 * handles the whoami API
 * @param sessionIndex the client's session index
 * @return a JSON string with the client's username
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

/**
 * sorts and selects appropriate handler if a server request is an API request
 * @param path the path of the request
 * @param sessionIndex the client's session index
 * @return true if request is handled as an API request, false otherwise
 */

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

/**
 * handles redirection of a client if needed to force login
 * @param path the path of the request
 * @param sessionIndex the client's session index
 * @return true if request is handled with a redirect, false otherwise
 */

bool handleRedirects(String path, int sessionIndex) {
  if (sessionIndex == -1 && !path.endsWith(".js") && !path.endsWith(".css") && path != "/login.html" && path != "/api/login") {
    sendRedirect(307, "login.html");
  } else return false;
  return true;
}

/**
 * sorts and selects appropriate handler for all server requests, serves static files
 * @param the path of the request
 * @return true if request is handled, false otherwise
 */

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

/**
 * sends a message to all authenticated websocket clients except for one (if needed)
 * @param message the message to send
 * @param excludeIndex the session index of the client that should be excluded (int < 0 to send to all clients)
 */

void wsSendToConnected(String message, int excludeIndex) {
  for (int i = 0; i < sessionCount; i++) {
    int sendToNum = sessions[i][2].toInt();
    if (sendToNum != -1 && i != excludeIndex) {
      wsServer.sendTXT(sendToNum, message);
    }
  }
}

/**
 * creates a list of authenticated websocket clients except for one (if needed)
 * @param path the path of the request
 * @param excludeIndex the session index of the client that should be excluded (int < 0 to include all clients)
 * @return a semicolon separated string of authenticated users' usernames
 */

String wsUserlist(int excludeIndex) {
  String userlist = "";
  bool notFirst = false;
  for (int i = 0; i < sessionCount; i++) {
    int sendToNum = sessions[i][2].toInt();
    if (sendToNum != -1 && i != excludeIndex) {
      if (notFirst) userlist += ";";
      else notFirst = true;
      userlist += sessions[i][1];
    }
  }
  return userlist;
}

/**
 * handles the websocket chat protocol (broadcasts messages)
 * @param command the chat protocol command
 * @param body the chat protocol body
 * @param num a websocket client num
 */

void wsHandleProtocolChat(String command, String body, uint8_t num) {
  if (command == "message") {
    int sessionIndex = sessionIndexFromWebsocket(num);
    String message = "chat:message:" + sessions[sessionIndex][1] + ";" + body;
    wsSendToConnected(message, -1);
  }
}

/**
 * disconnects an authenticated websocket client
 * @param sessionIndex the client's sessionIndex
 */

void wsDisconnect(int sessionIndex) {
  if (sessionIndex != -1) sessions[sessionIndex][2] = String(-1);
  String message = "global:disconnect:" + sessions[sessionIndex][1];
  wsSendToConnected(message, sessionIndex);
}

/**
 * handles the websocket global protocol (connection, authentication and disconnection)
 * @param command the global protocol command
 * @param body the global protocol body
 * @param num a websocket client num
 */

void wsHandleProtocolGlobal(String command, String body, uint8_t num) {
  if (command == "connect") {
    int sessionIndex = sessionIndexFromID(body);
    if (sessionIndex == -1) {
      wsServer.sendTXT(num, "global:error:connectionFailed");
    } else {
      sessions[sessionIndex][2] = String(num);
      String userlist = wsUserlist(sessionIndex);
      if (userlist.length() > 0) wsServer.sendTXT(num, "global:connect:" + userlist);
      String message = "global:connect:" + sessions[sessionIndex][1];
      wsSendToConnected(message, sessionIndex);
    }
  } else if (command == "disconnect") {
    int sessionIndex = sessionIndexFromID(body);
    if (sessionIndex != -1) {
      wsDisconnect(sessionIndex);
    }
  } else {
    wsServer.sendTXT(num, "global:error:pleaseAuthenticate");
  }
}

/**
 * main websocket event handler (connect, disconnect, and text payloads)
 * @param num a websocket client num (sequential unique identifier for websocket client)
 * @param type a websocket event type
 * @param payload a websocket event payload
 * @param length the length of the websocket event payload
 */

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      break;
    case WStype_DISCONNECTED: {
        int sessionIndex = sessionIndexFromWebsocket(num);
        if (sessionIndex != -1) wsDisconnect(sessionIndex);
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
        if (protocol == "global") wsHandleProtocolGlobal(command, body, num);
      } else {
        if (protocol == "global") wsHandleProtocolGlobal(command, body, num);
      }
      break;
  }
}

/*
Setup and loop functions
*/

/**
 * runs necessary functions for server setup
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

/**
 * the server loop (runs repeatedly to handle requests)
 */

void loop() {
  server.handleClient();
  wsServer.loop();
}
