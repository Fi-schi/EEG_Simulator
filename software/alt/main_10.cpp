#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <vector>
#include <cctype>  // Für isdigit()

// Netzwerkzugangsdaten
const char* ssid = "EEG-Simulator";      // WLAN-SSID
const char* password = "123456789";      // WLAN-Passwort

// Webserver und DNS-Server
AsyncWebServer server(80);
DNSServer dnsServer;

// Struktur zur Speicherung von Dateidaten (Name und Inhalt)
struct FileData {
  String filename; // Dateiname (z. B. "/Datei.txt")
  String content;  // Dateiinhalt
};

std::vector<FileData> uploadedFiles; // Container für hochgeladene Dateien

// Funktionsdefinitionen
void setupWiFi();
void setupFileSystem();
void setupWebServer();
String generateUniqueFileName(const String& baseName);
String getUploadedFilesList();
String processSingleFile(const String &content, const String &channel);
String parseChannelForFile(const String &channelsJson, const String &filename);

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
  
  // Umleitung aller unbekannten Anfragen
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
  
  // Upload-Route: Hier schreiben wir die Datei in SPIFFS
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Upload erfolgreich");
    },
    [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
      static String path;
      if (!index) {
        path = "/" + generateUniqueFileName(filename);
        // Öffne Datei im Schreibmodus
        request->_tempFile = SPIFFS.open(path, "w");
      }
      if (request->_tempFile) {
        request->_tempFile.write(data, len);
      }
      if (final) {
        request->_tempFile.close();
        // Speichere Dateidaten im Vektor
        FileData fd;
        fd.filename = path;  // z. B. "/Datei.txt"
        // Inhalt wird später über den SPIFFS ausgelesen – hier genügt der Dateiname.
        uploadedFiles.push_back(fd);
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
  server.on("/processFiles", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Erwarteter Parameter "channels" als JSON-Array (z. B. [{"name":"/Datei.txt","channel":"CH_2"}, ...])
      String channelsJson = "";
      if (request->hasParam("channels", true)) {
        channelsJson = request->getParam("channels", true)->value();
      }
      
      String finalResult = "";
      for (size_t i = 0; i < uploadedFiles.size(); i++) {
        // Ermittele für jede Datei den zugehörigen Channel
        String fileChannel = parseChannelForFile(channelsJson, uploadedFiles[i].filename);
        finalResult += "Datei: " + uploadedFiles[i].filename + "\n";
        // Lese den Inhalt aus SPIFFS
        File file = SPIFFS.open(uploadedFiles[i].filename, "r");
        if (file) {
          String content = file.readString();
          file.close();
          // Verarbeite den Inhalt und führe zusätzlich eine Kontrollprüfung durch
          String processed = processSingleFile(content, fileChannel);
          // Kontrolle: Zähle die Ziffern im Originalinhalt
          int digitCount = 0;
          for (size_t j = 0; j < content.length(); j++) {
            if (isdigit(content[j])) digitCount++;
          }
          // Extrahierte Zahlen werden in processSingleFile in einem String ausgegeben.
          // Wir können die Anzahl der Zahlen aus dem Ergebnis nicht direkt extrahieren,
          // deshalb zeigen wir hier zusätzlich die erwartete Anzahl (digitCount) an.
          processed += "Kontrolle: Es wurden " + String(digitCount) + " Ziffern gefunden.\n";
          finalResult += processed + "\n";
        } else {
          finalResult += "Fehler beim Öffnen der Datei.\n\n";
        }
      }
      // Optional: Lösche den Vektor, wenn die Dateien verarbeitet wurden.
      uploadedFiles.clear();
      request->send(200, "text/plain", finalResult);
    }
  );
  
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

// Verarbeitet den Inhalt einer einzelnen Datei für den angegebenen Channel
// und gibt einen Ergebnis-String zurück, der die extrahierten Zahlen enthält.
String processSingleFile(const String &content, const String &channel) {
  String result = "Verarbeitete Zahlen für " + channel + ":\n";
  std::vector<int> numbers;
  for (size_t i = 0; i < content.length(); i++) {
    if (isdigit(content[i])) {
      int num = content[i] - '0';
      numbers.push_back(num);
    }
  }
  for (size_t i = 0; i < numbers.size(); i++) {
    result += String(numbers[i]) + " ";
  }
  result += "\n";
  Serial.println(result);
  return result;
}

// Eine einfache Funktion zum Parsen eines JSON-Strings, um den Channel für eine Datei zu ermitteln.
// Erwartetes Format: [{"name":"/Datei.txt","channel":"CH_2"}, {"name":"/Datei2.txt","channel":"CH_3"}]
String parseChannelForFile(const String &channelsJson, const String &filename) {
  // Standardwert
  String fileChannel = "CH_1";
  int pos = channelsJson.indexOf("\"name\":\"" + filename + "\"");
  if (pos != -1) {
    int channelPos = channelsJson.indexOf("\"channel\":\"", pos);
    if (channelPos != -1) {
      channelPos += 11; // Länge von "\"channel\":\""
      int endPos = channelsJson.indexOf("\"", channelPos);
      if (endPos != -1) {
        fileChannel = channelsJson.substring(channelPos, endPos);
      }
    }
  }
  return fileChannel;
}
