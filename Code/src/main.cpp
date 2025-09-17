#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include "Global_Var.hpp"
#include "Server.hpp"
#include "PinMapping.hpp"
#include "Spannungswandlung.hpp"

// Globale Serverinstanz
AsyncWebServer server(80);

std::map<String, std::vector<float>> kanalDaten;
std::vector<FileData> uploadedFiles;
std::map<AsyncWebServerRequest*, ActiveUpload> activeUploads;

extern void ladeFrequenzAusDatei();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Pin Mapping initialisieren...");
  initPinModes();
  delay(100);
  Serial.println("DACs initialisieren...");
  digitalWrite(RST, LOW); // Reset des DACs
  delayMicroseconds(1);          // Setup-Zeit (tWS ≥ 0 ns)
  digitalWrite(RST, HIGH); // Reset des DACs beenden
  delay(100); // Warten, bis der Reset abgeschlossen ist
  Serial.println("Starte Netzwerkstack...");
  esp_netif_init();
  esp_event_loop_create_default();
  Serial.println("Initialisiere WiFi...");
  WiFi.mode(WIFI_AP);
  delay(250);  // wichtig!
  bool ok = WiFi.softAP("EEGsimulator", "EEGsimulator2525");
  delay(250);
  if (!ok) {
    Serial.println("❌ SoftAP-Start fehlgeschlagen!");
    while (true);
  }
  Serial.println("✅ SoftAP IP: " + WiFi.softAPIP().toString());
 Serial.println("WiFi Passwort: EEGsimulator2525");

  Serial.println("Initialisiere SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS Fehler");
    while (true);
  }

  delay(100);

  Serial.println("Lade Frequenz aus Datei...");
  ladeFrequenzAusDatei();

  delay(100);


  Serial.println("Starte Webserver...");
  setupWebServer();
  server.begin();
  Serial.println("✅ Webserver aktiv.");
}

void loop() {
  delay(10); // Leerlauf mit RTOS-Kooperation
}
