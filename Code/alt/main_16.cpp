#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <vector>
#include <map>
#include <cctype>    // Für isdigit()
#include <ArduinoJson.h>
#include <regex>     // Für die Regex-basierte Methode
#include <cmath>     // Für fabs()

// Netzwerkzugangsdaten
const char* ssid = "EEG-Simulator";      // WLAN-SSID
const char* password = "123456789";      // WLAN-Passwort

// Webserver und DNS-Server
AsyncWebServer server(80);
DNSServer dnsServer;

// Struktur zur Speicherung von Dateidaten (Name und Inhalt)
struct FileData {
  String filename; // z. B. "/myFile.txt"
  // Weitere Informationen können hier ergänzt werden
};

std::vector<FileData> uploadedFiles; // Container für hochgeladene Dateien

// Für parallele Uploads: Struktur zum Zwischenspeichern eines aktiven Uploads
struct ActiveUpload {
  File file;
  String path;
};
std::map<AsyncWebServerRequest*, ActiveUpload> activeUploads;

// Funktionsdefinitionen
void setupWiFi();
void setupFileSystem();
void setupWebServer();
String generateUniqueFileName(const String& baseName);
String getUploadedFilesList();
std::vector<float> extractNumbers(const String &content);
std::vector<float> extractNumbersRegex(const String &content);

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 startet...");

  setupWiFi();
  setupFileSystem();
  dnsServer.start(53, "*", WiFi.softAPIP());
  setupWebServer();
  Serial.println("Setup abgeschlossen. ESP32 bereit.");
}

void loop() {
  dnsServer.processNextRequest(); // DNS-Anfragen verarbeiten
}

void setupWiFi() {
  WiFi.softAP(ssid, password);
  Serial.println("WiFi gestartet");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());
}

void setupFileSystem() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Fehler: SPIFFS konnte nicht gestartet werden!");
    return;
  }
  Serial.println("SPIFFS erfolgreich gestartet.");
}

void setupWebServer() {
  // Hauptseite laden
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/HTML_Server.html")) {
      request->send(SPIFFS, "/HTML_Server.html", "text/html");
    } else {
      request->send(404, "text/plain", "Fehler: HTML_Server.html nicht gefunden.");
    }
  });
  
  // Alle unbekannten Anfragen umleiten
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  
  // Captive Portal Umleitungen
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204, "text/html", "");
  });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  
  // Logo abrufen
  server.on("/HS-Wismar_Logo-FIW_V1_RGB.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/HS-Wismar_Logo-FIW_V1_RGB.png")) {
      request->send(SPIFFS, "/HS-Wismar_Logo-FIW_V1_RGB.png", "image/png");
    } else {
      request->send(404, "text/plain", "Fehler: Logo nicht gefunden.");
    }
  });
  
  // Speicherinformationen
  server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    String json = "{";
    json += "\"total\": " + String(totalBytes) + ",";
    json += "\"used\": " + String(usedBytes);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // Upload-Route: Schreibe Datei in SPIFFS und speichere den Dateinamen
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Upload erfolgreich");
    },
    // Upload Handler
    [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
      // Beim ersten Chunk: eindeutigen Dateinamen generieren und Datei öffnen
      if (index == 0) {
        String path = "/" + generateUniqueFileName(filename);
        File file = SPIFFS.open(path, "w");
        if (!file) {
          Serial.println("Fehler beim Öffnen der Datei: " + path);
          return;
        }
        ActiveUpload au;
        au.file = file;
        au.path = path;
        activeUploads[request] = au;
      }
      // Schreibe Daten in die temporäre Datei
      if (activeUploads.find(request) != activeUploads.end()) {
        activeUploads[request].file.write(data, len);
      }
      // Beim letzten Chunk: Datei schließen und zur globalen Liste hinzufügen
      if (final) {
        if (activeUploads.find(request) != activeUploads.end()) {
          activeUploads[request].file.close();
          FileData fd;
          fd.filename = activeUploads[request].path;
          uploadedFiles.push_back(fd);
          activeUploads.erase(request);
          Serial.println("Datei gespeichert: " + fd.filename);
        }
      }
    }
  );
  
  // Route zum Abrufen der Dateiliste
  server.on("/getFiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    String filesList = getUploadedFilesList();
    request->send(200, "application/json", filesList);
  });
  
  // Löschen von Dateien
  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
      request->send(400, "text/plain", "Fehler: Kein Dateiname angegeben.");
      return;
    }
    String fileName = "/" + request->getParam("name")->value();
    if (SPIFFS.exists(fileName)) {
      SPIFFS.remove(fileName);
      request->send(200, "text/plain", "Datei erfolgreich gelöscht.");
    } else {
      request->send(404, "text/plain", "Datei nicht gefunden.");
    }
  });
  
  // Endpunkt zur Verarbeitung von Dateien
  server.on("/processFiles", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Erwarteter Parameter "channels" als JSON-Array, z. B.:
    // [{"name":"myFile.txt","channel":"CH_2"},{"name":"another.txt","channel":"CH_3"}]
    String channelsJson = "";
    if (request->hasParam("channels", true)) {
      channelsJson = request->getParam("channels", true)->value();
    }
    
    Serial.println("Empfangene channels JSON:");
    Serial.println(channelsJson);
    
    // Erstelle ein JSON-Dokument für die Antwort
    StaticJsonDocument<2048> resultDoc;
    JsonArray results = resultDoc.createNestedArray("results");
    
    // Verwende ArduinoJson, um das übermittelte JSON-Array zu parsen
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, channelsJson);
    if (error) {
      Serial.print("JSON Parse Error: ");
      Serial.println(error.c_str());
      request->send(400, "text/plain", "Fehler beim Parsen des channels JSON.");
      return;
    }
    
    JsonArray channelsArray = doc.as<JsonArray>();
    // Für jedes übermittelte Element (in der gewünschten Reihenfolge)
    for (JsonObject elem : channelsArray) {
      const char* name = elem["name"];
      const char* channel = elem["channel"];
      String filePath = "/" + String(name);
      
      JsonObject res = results.createNestedObject();
      res["filename"] = filePath;
      res["channel"] = channel;
      
      if (SPIFFS.exists(filePath)) {
        File file = SPIFFS.open(filePath, "r");
        if (file) {
          String content = file.readString();
          file.close();
          
          // Wende beide Methoden zur Zahlenerkennung an
          std::vector<float> numbers1 = extractNumbers(content);
          std::vector<float> numbers2 = extractNumbersRegex(content);
          
          // Prüfe auf leere Datei oder keine Zahlen gefunden
          if (content.length() == 0 || (numbers1.empty() && numbers2.empty())) {
            res["selfCheck"] = "Keine Zahlen gefunden. Datei übersprungen.";
            Serial.println("Datei " + filePath + " übersprungen: Keine Zahlen gefunden.");
            continue; // Überspringe weitere Verarbeitung dieser Datei
          }
          
          // Vergleiche, ob beide Methoden exakt identische Ergebnisse liefern
          bool equal = (numbers1.size() == numbers2.size());
          if (equal) {
            for (size_t i = 0; i < numbers1.size(); i++) {
              if (fabs(numbers1[i] - numbers2[i]) > 0.0001) { // Toleranz
                equal = false;
                break;
              }
            }
          }
          
          if (equal) {
            JsonArray nums = res.createNestedArray("numbers");
            for (size_t i = 0; i < numbers1.size(); i++) {
              nums.add(numbers1[i]);
            }
            res["numberCount"] = (int)numbers1.size();
            res["selfCheck"] = "OK";
          } else {
            res["selfCheck"] = "Fehler: Unterschiedliche Ergebnisse bei der Zahlenerkennung. Datei übersprungen.";
            Serial.println("Fehler bei Selbstkontrolle: Unterschiedliche Zahlenerkennungsergebnisse in Datei " + filePath);
            continue; // Überspringe diese Datei
          }
        } else {
          res["error"] = "Fehler beim Öffnen der Datei.";
          res["selfCheck"] = "Fehler";
        }
      } else {
        res["error"] = "Datei nicht gefunden.";
        res["selfCheck"] = "Fehler";
      }
    }
    
    String response;
    serializeJson(resultDoc, response);
    request->send(200, "application/json", response);
    // Nach der Verarbeitung können wir die Liste zurücksetzen, falls gewünscht
    uploadedFiles.clear();
  });
  
  server.begin();
  Serial.println("Webserver gestartet.");
}

String generateUniqueFileName(const String& baseName) {
  String uniqueName = baseName;
  int counter = 1;
  while (SPIFFS.exists("/" + uniqueName)) {
    uniqueName = baseName + "(" + String(counter++) + ")";
  }
  return uniqueName;
}

String getUploadedFilesList() {
  String filesList = "{\"files\":[";
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  while (file) {
    if (!first) {
      filesList += ",";
    }
    String name = String(file.name());
    // Für die Anzeige entfernen wir den führenden "/"
    if (name.startsWith("/")) {
      name = name.substring(1);
    }
    filesList += "\"" + name + "\"";
    first = false;
    file = root.openNextFile();
  }
  filesList += "]}";
  return filesList;
}

// Ursprüngliche Methode: Extrahiert zusammenhängende Zahlen (inklusive Fließkomma) aus einem String.
// Unterstützt negative Zahlen und einen einzelnen Dezimalpunkt.
std::vector<float> extractNumbers(const String &content) {
  std::vector<float> numbers;
  int i = 0;
  while (i < content.length()) {
    // Überspringe Zeichen, die kein Minus, Ziffer oder Dezimalpunkt sind
    while (i < content.length() && !(isdigit(content[i]) || content[i] == '-' || content[i] == '.')) {
      i++;
    }
    if (i >= content.length()) break;
    
    int start = i;
    bool dotFound = false;
    // Optionales Minuszeichen
    if (content[i] == '-') {
      i++;
    }
    // Verarbeite die Ziffern und einen optionalen Dezimalpunkt
    while (i < content.length() && (isdigit(content[i]) || (content[i] == '.' && !dotFound))) {
      if (content[i] == '.') {
        dotFound = true;
      }
      i++;
    }
    String numStr = content.substring(start, i);
    // Umwandlung in float
    float value = numStr.toFloat();
    numbers.push_back(value);
  }
  return numbers;
}

// Alternative Methode: Extraktion von Zahlen mittels Regex
std::vector<float> extractNumbersRegex(const String &content) {
  std::vector<float> numbers;
  std::string s = content.c_str();
  // Regex: Optionales Minuszeichen, gefolgt von Ziffern, optional einem Dezimalpunkt und weiteren Ziffern
  std::regex floatRegex("[-]?[0-9]*\\.?[0-9]+");
  auto numbersBegin = std::sregex_iterator(s.begin(), s.end(), floatRegex);
  auto numbersEnd = std::sregex_iterator();
  for (std::sregex_iterator i = numbersBegin; i != numbersEnd; ++i) {
    std::smatch match = *i;
    try {
      float value = std::stof(match.str());
      numbers.push_back(value);
    } catch (...) {
      // Falls die Umwandlung fehlschlägt, ignoriere diesen Treffer
    }
  }
  return numbers;
}
