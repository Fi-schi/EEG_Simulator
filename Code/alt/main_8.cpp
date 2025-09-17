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

// Globaler Container für den Inhalt der hochgeladenen Dateien
std::vector<String> uploadedFilesContent;

// Globale Arrays für die Channels
std::vector<int> CH_1, CH_2, CH_3, CH_4;

// Funktionsdefinitionen
void setupWiFi();
void setupFileSystem();
void setupWebServer();
String generateUniqueFileName(const String& baseName);
String getUploadedFilesList();
// Diese Funktion filtert alle Nicht-Ziffern, speichert die Ziffern in das jeweilige Array
// und baut einen Ergebnis-String auf, der zurückgegeben wird.
String processFileContent(const String& content, const String& channel);

void setup() {
    Serial.begin(115200);
    Serial.println("\nESP32 startet...");

    // WLAN im Access Point-Modus starten
    setupWiFi();

    // SPIFFS-Dateisystem initialisieren
    setupFileSystem();

    // DNS-Server starten und alle Domains umleiten
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Webserver einrichten
    setupWebServer();

    Serial.println("Setup abgeschlossen. ESP32 bereit.");
}

void loop() {
    dnsServer.processNextRequest(); // DNS-Anfragen verarbeiten
}

void setupWiFi() {
    WiFi.softAP(ssid, password); // Access Point starten
    Serial.println("WiFi gestartet");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.softAPIP());
}

void setupFileSystem() {
    if (!SPIFFS.begin(true)) { // SPIFFS mit Formatierung erzwingen, falls nicht vorhanden
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

    // Standardroute: Umleitung auf die Hauptseite
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");  // Alle unbekannten Anfragen zu / umleiten
    });

    // Captive Portal-Umleitung für verschiedene Geräte
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204, "text/html", "");  // Android Captive Portal
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/"); // iOS Captive Portal
    });

    // Logo-Datei abrufen
    server.on("/HS-Wismar_Logo-FIW_V1_RGB.png", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/HS-Wismar_Logo-FIW_V1_RGB.png")) {
            request->send(SPIFFS, "/HS-Wismar_Logo-FIW_V1_RGB.png", "image/png");
        } else {
            request->send(404, "text/plain", "Fehler: Logo nicht gefunden.");
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

    // Route zum Hochladen von Dateien (wie bisher)
    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Upload erfolgreich");
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
            static String path;
            if (!index) {
                path = "/" + generateUniqueFileName(filename);
                request->_tempFile = SPIFFS.open(path, "w");
            }
            if (request->_tempFile) {
                request->_tempFile.write(data, len);
            }
            if (final) {
                request->_tempFile.close();
            }
        });

    // Route zum Abrufen der Dateiliste
    server.on("/getFiles", HTTP_GET, [](AsyncWebServerRequest *request) {
        String filesList = getUploadedFilesList();
        request->send(200, "application/json", filesList);
    });

    // Route zum Löschen von Dateien
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

    // Angepasster Endpunkt zum Verarbeiten von Dateien
    server.on("/processFiles", HTTP_POST,
        // Finaler Callback, der nach Abschluss aller Uploads aufgerufen wird
        [](AsyncWebServerRequest *request) {
            // Hier wird für dieses Beispiel der Standardchannel genutzt.
            // Die übermittelten JSON-Daten unter "channels" könnten geparst werden, um für jede Datei den zugehörigen Channel zu bestimmen.
            String channel = "CH_1"; // Standardchannel
            String allContents = "";
            for (size_t i = 0; i < uploadedFilesContent.size(); i++) {
                allContents += "Datei " + String(i+1) + ":\n" + uploadedFilesContent[i] + "\n\n";
            }
            String processedOutput = processFileContent(allContents, channel);
            uploadedFilesContent.clear();
            request->send(200, "text/plain", processedOutput);
        },
        // Upload-Callback: wird für jede hochgeladene Datei aufgerufen
        [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
            static String fileContent;
            if (!index) {
                fileContent = "";
            }
            fileContent += String((char*)data);
            if (final) {
                uploadedFilesContent.push_back(fileContent);
            }
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

String processFileContent(const String& content, const String& channel) {
    String result = "Verarbeitete Zahlen für " + channel + ":\n";
    if (channel == "CH_1") {
        CH_1.clear();
    } else if (channel == "CH_2") {
        CH_2.clear();
    } else if (channel == "CH_3") {
        CH_3.clear();
    } else if (channel == "CH_4") {
        CH_4.clear();
    } else {
        result = "Unbekannter Channel: " + channel;
        return result;
    }
    for (size_t i = 0; i < content.length(); i++) {
        if (isdigit(content[i])) {
            int num = content[i] - '0';
            if (channel == "CH_1") {
                CH_1.push_back(num);
            } else if (channel == "CH_2") {
                CH_2.push_back(num);
            } else if (channel == "CH_3") {
                CH_3.push_back(num);
            } else if (channel == "CH_4") {
                CH_4.push_back(num);
            }
        }
    }
    if (channel == "CH_1") {
        for (size_t i = 0; i < CH_1.size(); i++) {
            result += String(CH_1[i]) + " ";
        }
    } else if (channel == "CH_2") {
        for (size_t i = 0; i < CH_2.size(); i++) {
            result += String(CH_2[i]) + " ";
        }
    } else if (channel == "CH_3") {
        for (size_t i = 0; i < CH_3.size(); i++) {
            result += String(CH_3[i]) + " ";
        }
    } else if (channel == "CH_4") {
        for (size_t i = 0; i < CH_4.size(); i++) {
            result += String(CH_4[i]) + " ";
        }
    }
    Serial.println(result);
    return result;
}
