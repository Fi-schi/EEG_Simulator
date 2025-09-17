#ifndef SERVER_HPP
#define SERVER_HPP

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

#include "Global_Var.hpp"
#include "PinMapping.hpp"
#include "Spannungswandlung.hpp"

// Funktionsprototypen
void setupWebServer();
void setupRoutes(AsyncWebServer& server);

std::vector<float> extractNumbersRegex(const String &content);
String generateUniqueFileName(const String& baseName);
String getUploadedFilesList();

#endif // SERVER_HPP
