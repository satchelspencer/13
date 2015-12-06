#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <ESP8266TrueRandom.h>
#include <Hash.h>
#include <WebSocketsServer.h>

const char WifiAPSSID[] = "tttt";
IPAddress WifiAPIP(192, 168, 1, 1);
const char WifiDOMAIN[] = "192.168.1.1";
const char ServerURL[] = "http://192.168.1.1/";
const int MAX_SESSIONS = 4;
String sessions[MAX_SESSIONS][2];
int sessionCount = 0;
int websockets[MAX_SESSIONS]; //contains websocet numbers given a session index
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
  return sessionCount++;
}

void removeSession(int sessionIndex) {
  if (sessionIndex >= sessionCount) return;
  for (int i = sessionIndex; i < sessionCount - 1; i++) {
    sessions[i][0] = sessions[i + 1][0];
    sessions[i][1] = sessions[i + 1][1];
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
    if (websockets[i] == num) return i;
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

bool handleAPIRequest(String path, int sessionIndex) {
  if (!(path.startsWith("/api/"))) return false;
  path = path.substring(5);
  String returnJSON;
  if (path == "login") returnJSON = handleLogin();
  else if (path == "logout") returnJSON = handleLogout(sessionIndex);
  else return false;
  server.send(200, "application/json", returnJSON);
  return true;
}

// I'll probably end up getting rid of this one soon in favor of some sort of permission system
bool handleAdminRequest(String path, int sessionIndex) {
  if (!(path.startsWith("/admin/")) || sessions[sessionIndex][1] != "admin") return false;
  path = path.substring(7);
  if (path == "sessions.html") {
    String message = "<!DOCTYPE html><html><head><title>Sessions</title><style>table,th,td{border:1px solid black;border-collapse:collapse;}th,td{padding:5px;}</style></head><body><h1>Sessions</h1><table style=\"width:100%\"><tr><th>Session ID</th><th>Username</th></tr>";
    for (int i = 0; i < sessionCount; i++) {
      message += "<tr><td>" + sessions[i][0] + "</td><td>" + sessions[i][1] + "</td></tr>";
    }
    message += "</table></body></html>";
    server.send(200, "text/html", message);
  }/* else if (path == "WebSocketTest.html") {
    File f = openFileR(path);
    if (f) {
      String contentType = getContentType(path);
      server.streamFile(f, contentType);
      f.close();
    }
  }*/ else return false;
  return true;
}

bool handleRedirects(String path, int sessionIndex) {
  if (path == "/") {
    sendRedirect(307, "dashboard.html");
  } else if (path == "/dashboard.html" && sessionIndex == -1) {
    sendRedirect(303, "login.html");
  } else if (path == "/login.html" && sessionIndex != -1) {
    sendRedirect(303, "dashboard.html");
  } else return false;
  return true;
}

bool handleRequest(String path) {
  int sessionIndex = sessionIndexFromCookie();
  if (handleRedirects(path, sessionIndex)) return true;
  if (handleAdminRequest(path, sessionIndex)) return true;
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
    if (path == "/dashboard.html" || path == "/chat.html") {
      String htmlString = f.readString();
      htmlString.replace("{{username}}", sessions[sessionIndex][1]);
      server.send(200, contentType, htmlString);
    } else server.streamFile(f, contentType);
    f.close();
    return true;
  }
  return false;
}

/*
WebSocket functions
*/

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      break;
    case WStype_DISCONNECTED:
      break;
    case WStype_TEXT:
      String payloadString = String((char *)payload)+":";
      String data[3];
      int startIndex = 0;
      int index = payloadString.indexOf(":");
      int pieces = 0;
      while (index != -1) {
        String piece = payloadString.substring(startIndex, index);
        data[pieces++] = piece;
        startIndex = index + 1;
        index = payloadString.indexOf(":", startIndex);
      }
      /* 
       *  data[0] is protocol
       *  data[1] is command
       *  data[2] is body
       */
      if(data[0] == "global" && data[1] == "connect"){
        int index = sessionIndexFromID(data[2]); //get session index 
        if(index != -1){
          websockets[index] = num; //associate session with websocket number
           wsServer.sendTXT(num, "global:connect:success");
        }else wsServer.sendTXT(num, "global:connect:fail");
      }else{
        int sessionIndex = sessionIndexFromWebsocket(num);
        if(sessionIndex == -1) wsServer.sendTXT(num, "not authenticated");
        else{
          /* authenticated, catch commands here */
          if(data[0] == "chat" && data[1] == "message"){
            for (int i = 0; i < sessionCount; i++) {
              wsServer.sendTXT(websockets[i], "chat:message:"+sessions[i][1]+":"+data[2]);
            }
          }
        }
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
  WiFi.mode(WIFI_OFF);
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
