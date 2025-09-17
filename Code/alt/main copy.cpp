#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Netzwerkzugangsdaten
const char* ssid = "EEG-Simulator";
const char* password = "123456789";

// Webserver-Port
AsyncWebServer server(80);

// Flag zur Bearbeitung der ausgewählten Datei
volatile bool processFileFlag = false;
String selectedFile = "";

// Funktionsdefinitionen
void startWiFi();
void startWebServer();
String listFiles();
void setupFileSystem();
void fileUploadHandler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleFileProcessing(void *pvParameters);
void processFile(String filename);

// FreeRTOS Task-Handler
TaskHandle_t fileProcessingTask;

/*void setup() {
    Serial.begin(115200);

    // WiFi und Webserver starten
    startWiFi();
    startWebServer();

    // Dateisystem einrichten
    setupFileSystem();

    // Start der FreeRTOS-Task zur Verarbeitung der Datei
    xTaskCreatePinnedToCore(
        handleFileProcessing,  // Task-Funktion mit Parameter
        "fileProcessingTask",
        8192,
        NULL,
        1,
        &fileProcessingTask,
        1
    );

}*/

void setup() {
    Serial.begin(115200);
    Serial.println("Setup gestartet...");
    
    startWiFi();
    Serial.println("WiFi gestartet...");
    
    startWebServer();
    Serial.println("Webserver gestartet...");
    
    setupFileSystem();
    Serial.println("Dateisystem eingerichtet...");
    
    // Task starten
    xTaskCreatePinnedToCore(
        handleFileProcessing,
        "fileProcessingTask",
        8192,
        NULL,
        1,
        &fileProcessingTask,
        1
    );
    Serial.println("Task erstellt...");
}


void loop() {
    // Die Hauptschleife bleibt leer, da die Aufgaben durch FreeRTOS und den asynchronen Webserver verarbeitet werden.
}

void startWiFi() {
    WiFi.softAP(ssid, password);
    Serial.println("WiFi gestartet");
    Serial.print("IP Adresse: ");
    Serial.println(WiFi.softAPIP());
}

void startWebServer() {
    // HTML-Seite für Datei-Upload und Anzeige
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), false);
    });

    // Datei-Upload Handler
    server.onFileUpload(fileUploadHandler);

    // Verarbeitung der ausgewählten Datei
    server.on("/process", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (selectedFile != "") {
            processFileFlag = true;
            request->send(200, "text/plain", "Verarbeitung gestartet");
        } else {
            request->send(400, "text/plain", "Keine Datei ausgewählt");
        }
    });

    // Dateiliste anzeigen
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/HTML_Server.html", "text/html");
    });


    server.begin();
    Serial.println("Webserver gestartet");
}

void setupFileSystem() {
    if (!SPIFFS.begin()) {
    Serial.printf("Fehler beim Start von SPIFFS. Fehlercode: %d\n", SPIFFS.begin());
    return;
    }

    Serial.println("SPIFFS erfolgreich gestartet");
}


String listFiles() {
    String output = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file) {
        if (output != "[") output += ",";
        output += "{\"name\":\"" + String(file.name()) + "\", \"size\":\"" + String(file.size()) + "\"}";
        file = root.openNextFile();
    }
    output += "]";
    return output;
}


void fileUploadHandler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Hochladen gestartet: %s\n", filename.c_str());
        request->_tempFile = SPIFFS.open("/" + filename, "w");
    }
    if (request->_tempFile) {
        request->_tempFile.write(data, len);
    }
    if (final) {
        request->_tempFile.close();
        Serial.printf("Hochladen abgeschlossen: %s\n", filename.c_str());
        request->send(200, "text/plain", "Datei hochgeladen");
    }
}

void handleFileProcessing(void *pvParameters) {
    while (true) {
        if (processFileFlag) {
            processFile(selectedFile);
            processFileFlag = false;
            selectedFile = ""; // Reset der Auswahl
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void processFile(String filename) {
    // Hier kann der Code zur Bearbeitung der Datei eingebaut werden
    Serial.printf("Verarbeitung der Datei: %s\n", filename.c_str());
    // Beispielhafte Verarbeitung
    File file = SPIFFS.open("/" + filename, "r");
    if (!file) {
        Serial.println("Datei konnte nicht geöffnet werden");
        return;
    }
    while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println("Gelesene Zeile: " + line);  // Beispiel: Zeilenweise Verarbeitung
    }
    file.close();
}
