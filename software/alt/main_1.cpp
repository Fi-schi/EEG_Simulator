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
    if (!SPIFFS.begin(true)) {  // SPIFFS mit Formatierung erzwingen
        Serial.println("Fehler: SPIFFS konnte nicht gestartet werden!");
        return;
    }
    Serial.println("SPIFFS erfolgreich gestartet.");
}

void listAllFiles() {
    Serial.println("Dateien im SPIFFS:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file) {
        Serial.printf("Datei: %s, Größe: %d bytes\n", file.name(), file.size());
        file = root.openNextFile();
    }
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

    // Route für das Logo (HS-Wismar_Logo-FIW_V1_RGB.png)
server.on("/HS-Wismar_Logo-FIW_V1_RGB.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Logo aufgerufen");
    
    // Überprüfe, ob die Datei im SPIFFS vorhanden ist
    if (SPIFFS.exists("/HS-Wismar_Logo-FIW_V1_RGB.png")) {
        // Sende das Logo mit dem korrekten MIME-Typ für PNG
        request->send(SPIFFS, "/HS-Wismar_Logo-FIW_V1_RGB.png", "image/png");
    } else {
        // Fehlerbehandlung, wenn die Datei nicht gefunden wurde
        Serial.println("Fehler: Logo nicht gefunden!");
        request->send(404, "text/plain", "Fehler: Logo nicht gefunden.");
    }
});


    // Test-Route für Debugging
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Test-Endpunkt erreicht");
        request->send(200, "text/plain", "Der Server funktioniert einwandfrei!");
    });

    // Datei-Upload-Route
    server.on("/upload", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Upload erfolgreich");
        }, 
        [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.printf("Upload gestartet: %s\n", filename.c_str());
                request->_tempFile = SPIFFS.open("/" + filename, "w");
            }
            if (request->_tempFile) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                Serial.printf("Upload abgeschlossen: %s\n", filename.c_str());
                request->_tempFile.close();
            }
        }
    );

    // Server starten
    server.begin();
    Serial.println("Webserver gestartet.");
}
