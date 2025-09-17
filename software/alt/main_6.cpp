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
void processFileContent(const String& content, const String& channel);

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
    // Hier werden die hochgeladenen Dateien über den Upload-Callback verarbeitet.
    server.on("/processFiles", HTTP_POST,
        // Finaler Callback, der nach Abschluss aller Uploads aufgerufen wird
        [](AsyncWebServerRequest *request) {
            String channel = "CH_1"; // Standardchannel
            if (request->hasParam("channel", true)) { // true => POST-Parameter
                channel = request->getParam("channel", true)->value();
            }
            // Kombiniere den Inhalt aller hochgeladenen Dateien
            String allContents = "";
            for (size_t i = 0; i < uploadedFilesContent.size(); i++) {
                allContents += "Datei " + String(i+1) + ":\n" + uploadedFilesContent[i] + "\n\n";
            }
            processFileContent(allContents, channel);
            // Leere den Container für zukünftige Uploads
            uploadedFilesContent.clear();
            request->send(200, "text/plain", "Dateien erfolgreich verarbeitet.");
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
        // Dateinamen ohne führenden Slash zurückgeben
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

// Verarbeitet den kombinierten Inhalt der hochgeladenen Dateien:
// Es werden alle Zeichen entfernt, die keine Ziffern sind, und die übrig gebliebenen Zahlen in das
// für den ausgewählten Channel entsprechende Array gespeichert.
void processFileContent(const String& content, const String& channel) {
    Serial.println("Verarbeite folgende Dateien für Channel " + channel + ":");
    Serial.println(content);

    // Hilfsfunktion: fügt aus dem Inhalt alle Ziffern in das gegebene Array ein
    auto processContentForChannel = [&](const String& content, std::vector<int>& channelArray) {
        for (size_t i = 0; i < content.length(); i++) {
            if (isdigit(content[i])) {
                int num = content[i] - '0';
                channelArray.push_back(num);
            }
        }
    };

    if (channel == "CH_1") {
        processContentForChannel(content, CH_1);
    } else if (channel == "CH_2") {
        processContentForChannel(content, CH_2);
    } else if (channel == "CH_3") {
        processContentForChannel(content, CH_3);
    } else if (channel == "CH_4") {
        processContentForChannel(content, CH_4);
    } else {
        Serial.println("Unbekannter Channel: " + channel);
    }
}
