#include "./Spannungswandlung.hpp"
#include "Global_Var.hpp"
#include "PinMapping.hpp"
#include <Arduino.h>

void ausgabe(char Channel, uint16_t Data) {
    //Serial.printf("Ausgabe auf Kanal %c: %u\n", Channel, Data);
    Data &= 0x0FFF; // Nur untere 12 Bit zulassen
  
    digitalWrite(R_W, LOW);        // Schreiben aktivieren
    digitalWrite(Load_Data, LOW);  // LDAC aktivieren
  
  
    // Adressleitungen A0, A1 setzen → DAC A–D
    switch(Channel) {
        case 'A':
            digitalWrite(ADD0, LOW);     // A0
            digitalWrite(ADD1, LOW);    // A1
            break;
        case 'B':
            digitalWrite(ADD0, HIGH);    // A0
            digitalWrite(ADD1, LOW);     // A1
            break;
        case 'C':
            digitalWrite(ADD0, LOW);    // A0
            digitalWrite(ADD1, HIGH);     // A1
            break;
        case 'D':
            digitalWrite(ADD0, HIGH);     // A0
            digitalWrite(ADD1, HIGH);    // A1
            break;
        default:
            Serial.println("Ungültiger Kanal!"); // Ungültiger Kanal
            return; // Funktion beenden
    }
  
    // Datenleitungen setzen (12 Bit)
    digitalWrite(DB0,  (Data >> 0)  & 0x01);
    digitalWrite(DB1,  (Data >> 1)  & 0x01);
    digitalWrite(DB2,  (Data >> 2)  & 0x01);
    digitalWrite(DB3,  (Data >> 3)  & 0x01);
    digitalWrite(DB4,  (Data >> 4)  & 0x01);
    digitalWrite(DB5,  (Data >> 5)  & 0x01);
    digitalWrite(DB6,  (Data >> 6)  & 0x01);
    digitalWrite(DB7,  (Data >> 7)  & 0x01);
    digitalWrite(DB8,  (Data >> 8)  & 0x01);
    digitalWrite(DB9,  (Data >> 9)  & 0x01);
    digitalWrite(DB10, (Data >> 10) & 0x01);
    digitalWrite(DB11, (Data >> 11) & 0x01);
  
    // Schreiben vorbereiten
    digitalWrite(CS, LOW);         // Chip aktivieren
    delayMicroseconds(1);          // Setup-Zeit (tWS ≥ 0 ns)
    digitalWrite(CS, HIGH);       // Lesen deaktivieren (Schreiben aktivieren)
   
  
    // Daten übernehmen (Load)
  
    //delayMicroseconds(1);        // tLDW ≥ 170 ns laut Datenblatt
    //digitalWrite(Load_Data, LOW); // LDAC deaktivieren
  
    // Schreiben beenden
   // digitalWrite(R_W, HIGH);       // Schreiben beenden (nicht zwingend nötig, aber klar)
    //digitalWrite(CS, HIGH);        // Chip deaktivieren
  
  }

// FreeRTOS Task Handle
TaskHandle_t abspielTaskHandle = nullptr;
extern int ausgabeFrequenzHz;

void abspielTask(void* parameter) {
    Serial.println("Starte Abspielen der Daten (FreeRTOS Task)...");

    // Hier definieren wir die Min- und Max-Werte für das EEG-Signal (in mV)
    float minEEG = -150.0f; // mV (Minimum des EEG-Signals)
    float maxEEG = 150.0f;  // mV (Maximum des EEG-Signals)

    // Maximale Länge aller Kanäle bestimmen
    size_t maxLength = 0;
    if (kanalDaten.empty()) {
        Serial.println("⚠️ Keine Kanal-Daten geladen. Task wird abgebrochen.");
        abspielTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    // Bestimmung der maximalen Datenlänge über alle Kanäle hinweg
    for (const auto& [_, data] : kanalDaten) {
        if (data.size() > maxLength) {
            maxLength = data.size();
        }
    }

    Serial.printf("Maximale Länge aller Kanäle: %zu\n", maxLength);
    if (maxLength == 0) {
        Serial.println("⚠️ Kanäle vorhanden, aber alle leer. Task wird beendet.");
        abspielTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    // Abspielen der Daten in einer Schleife
    for (size_t i = 0; i < maxLength; ++i) {
        for (const auto& [channel, data] : kanalDaten) {
            // Werte aus den geladenen Daten holen (mV)
            float value = (i < data.size()) ? data[i] : 0.0f;

            // Skalierung der Werte von -100 mV bis +100 mV auf den DAC-Bereich von 0 bis 4095
            // Der Mittelwert von 0 mV wird auf den Wert 2047 (Mitte des DAC-Bereichs) gesetzt
            float range = maxEEG - minEEG;
            if (range == 0) range = 1;  // Sicherheitsabfrage gegen Division durch 0

            // Normalisierung der Werte zwischen 0 und 1
            double normalized = (value - minEEG) / range;
            normalized = constrain(normalized, 0.0f, 1.0f);

            // Umrechnung auf den DAC-Bereich 0-4095
            uint16_t dacValue;
            dacValue = static_cast<uint16_t>(normalized * 4095.0f);


            // Ausgabe des Wertes in der Konsole zur Überprüfung
            Serial.print("Kanal ");
            Serial.print(channel);
            Serial.print(" → ");
            Serial.print(value, 2);  // Wert in mV
            Serial.print(" mV → DAC-Wert: ");
            Serial.println(dacValue);

            // Ausgabe des DAC-Werts an den entsprechenden Kanal
            ausgabe(channel[3], dacValue);

            // RTOS-Kooperation
            yield();
        }
        // Verzögerung, um die gewünschte Abspiel-Frequenz zu erreichen
        vTaskDelay(pdMS_TO_TICKS(1000 / ausgabeFrequenzHz));
    }

    Serial.println("Abspielen der Daten abgeschlossen.");
    abspielTaskHandle = nullptr;
    vTaskDelete(nullptr); // Task selbst löschen
}



// Startfunktion für den Task
void startAbspielTask() {
    if (abspielTaskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            abspielTask,         // Task-Funktion
            "AbspielTask",       // Name
            4096,                // Stack-Größe
            nullptr,             // Parameter
            1,                   // Priorität
            &abspielTaskHandle,  // Handle
            1                    // Core (1 = App Core auf ESP32)
        );
    } else {
        Serial.println("Abspiel-Task läuft bereits!");
    }
}
