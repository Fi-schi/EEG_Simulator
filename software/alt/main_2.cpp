#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// Netzwerkzugangsdaten
const char* ssid = "EEG-Simulator";      // WLAN-SSID
const char* password = "123456789";      // WLAN-Passwort

// Webserver-Port und Instanz
AsyncWebServer server(80);

// Funktionsdefinitionen
void setupWiFi();
void setupFileSystem();
void setupWebServer();
void listAllFiles();
String getFreeSpace();
String getUploadedFilesList();
String generateUniqueFileName(const String& baseName);

// Setup
void setup() {
    Serial.begin(115200);
    Serial.println("\nESP32 startet...");

    // WiFi starten
    setupWiFi();

    // SPIFFS-Dateisystem einrichten
    setupFileSystem();

    // Alle Dateien im SPIFFS auflisten (Debugging)
    listAllFiles();

    // Webserver einrichten
    setupWebServer();

    Serial.println("Setup abgeschlossen. ESP32 bereit.");
}

void loop() {
    // Hauptschleife bleibt leer, da Webserver und WiFi asynchron arbeiten
}

void setupWiFi() {
    WiFi.softAP(ssid, password);  // Access Point starten
    Serial.println("WiFi gestartet");
    Serial.print("IP Adresse: ");
    Serial.println(WiFi.softAPIP());  // AP-IP-Adresse ausgeben
}

void setupFileSystem() {
    if (!SPIFFS.begin(true)) {  // SPIFFS mit Formatierung erzwingen, falls nicht vorhanden
        Serial.println("Fehler: SPIFFS konnte nicht gestartet werden!");
        return;
    }
    Serial.println("SPIFFS erfolgreich gestartet.");
}

// Debugging: Dateiinhalte nach dem Speichern lesen
void listAllFiles() {
    Serial.println("Dateien im SPIFFS:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file) {
        Serial.printf("Datei: %s, Größe: %d bytes\n", file.name(), file.size());
        
        // Hier die ersten paar Bytes der Datei ausgeben, um zu überprüfen, ob der Inhalt stimmt
        String content = file.readStringUntil('\n'); // Beispiel: Ausgabe der ersten Zeile
        Serial.println(content);
        
        file = root.openNextFile();
    }
}
String generateUniqueFileName(const String& baseName) {
    String uniqueName = baseName;
    int counter = 1;
    while (SPIFFS.exists("/" + uniqueName)) {
        uniqueName = baseName + "(" + String(counter++) + ")";
    }
    return uniqueName;
}

String getFreeSpace() {
    uint32_t totalBytes = SPIFFS.totalBytes();
    uint32_t usedBytes = SPIFFS.usedBytes();
    uint32_t freeBytes = totalBytes - usedBytes;
    return String(freeBytes) + " Bytes frei von " + String(totalBytes) + " Bytes";
}

void setupWebServer() {
    // Route für die Hauptseite
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Hauptseite aufgerufen");
        if (SPIFFS.exists("/HTML_Server.html")) {
            request->send(SPIFFS, "/HTML_Server.html", "text/html");
        } else {
            Serial.println("Fehler: HTML_Server.html nicht gefunden!");
            request->send(404, "text/plain", "Fehler: HTML_Server.html nicht gefunden.");
        }
    });

    // Route für das Logo
    server.on("/HS-Wismar_Logo-FIW_V1_RGB.png", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Logo aufgerufen");
        if (SPIFFS.exists("/HS-Wismar_Logo-FIW_V1_RGB.png")) {
            request->send(SPIFFS, "/HS-Wismar_Logo-FIW_V1_RGB.png", "image/png");
        } else {
            Serial.println("Fehler: Logo nicht gefunden!");
            request->send(404, "text/plain", "Fehler: Logo nicht gefunden.");
        }
    });

server.on("/upload", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Upload erfolgreich");
    },
    [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        static String path;
        if (!index) {
            Serial.printf("Upload gestartet: %s\n", filename.c_str());
            path = "/" + generateUniqueFileName(filename); // Generiere eindeutigen Dateinamen
            request->_tempFile = SPIFFS.open(path, "w");
            if (!request->_tempFile) {
                Serial.printf("Fehler: Datei %s konnte nicht geöffnet werden.\n", filename.c_str());
                return;
            }
        }

        if (request->_tempFile) {
            request->_tempFile.write(data, len); // Daten schreiben
            Serial.printf("Daten geschrieben: %d Bytes\n", len);
        } else {
            Serial.println("Fehler: _tempFile ist NULL.");
        }

        if (final) {
            Serial.printf("Upload abgeschlossen: %s\n", path.c_str());
            request->_tempFile.close();
        }
    }
);

// Route zum Löschen einer Datei
server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
        request->send(400, "text/plain", "Fehler: Kein Dateiname angegeben.");
        return;
    }

    String fileName = "/" + request->getParam("name")->value(); // Dateiname abrufen
    if (SPIFFS.exists(fileName)) {
        if (SPIFFS.remove(fileName)) {
            Serial.printf("Datei gelöscht: %s\n", fileName.c_str());
            request->send(200, "text/plain", "Datei erfolgreich gelöscht.");
        } else {
            Serial.printf("Fehler beim Löschen der Datei: %s\n", fileName.c_str());
            request->send(500, "text/plain", "Fehler beim Löschen der Datei.");
        }
    } else {
        Serial.printf("Datei nicht gefunden: %s\n", fileName.c_str());
        request->send(404, "text/plain", "Datei nicht gefunden.");
    }
});

// Speicherinformationen abrufen
server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();

    String json = "{";
    json += "\"total\": " + String(totalBytes) + ",";
    json += "\"used\": " + String(usedBytes);
    json += "}";

    request->send(200, "application/json", json);
});

    // Route zum Abrufen der Dateiliste
    server.on("/getFiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        String filesList = getUploadedFilesList();
        request->send(200, "application/json", filesList);
    });

    server.begin();
    Serial.println("Webserver gestartet.");
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
        filesList += "\"" + String(file.name()) + "\""; // Dateinamen hinzufügen
        first = false;
        file = root.openNextFile();
    }

    filesList += "]}";
    return filesList;
}

