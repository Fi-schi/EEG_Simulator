#ifndef GLOBAL_VAR_HPP
#define GLOBAL_VAR_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <vector>
#include <map>
#include <ArduinoJson.h>
#include <regex>
#include <cmath>

// WiFi-Zugangsdaten
//extern const char* ssid;
//extern const char* password;

// Globale Serverinstanzen
extern AsyncWebServer server;
extern DNSServer dnsServer;

// Strukturdefinitionen (einmalig hier!)
struct FileData {
  String filename;
};

struct ActiveUpload {
  File file;
  String path;
};

// Globale Variablen
extern std::map<String, std::vector<float>> kanalDaten;
extern std::map<AsyncWebServerRequest*, ActiveUpload> activeUploads;
extern std::vector<FileData> uploadedFiles;

#endif // GLOBAL_VAR_HPP
