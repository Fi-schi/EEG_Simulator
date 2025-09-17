# EEG-Simulator

Ein Projektseminar an der Hochschule Wismar zur **Entwicklung eines
EEG-Signalgenerators**. 
Ziel ist es, eine kompakte, intuitive und didaktisch nutzbare Plattform
zur Erzeugung synthetischer EEG-Signale bereitzustellen.

## Features

-   4 analoge Ausgangskanäle für simultane Signalwiedergabe 
-   Steuerung über einen **ESP32-S3 Mikrocontroller** 
-   Hochauflösender DAC (**DAC8412FPZ**) mit 12-Bit Auflösung 
-   Benutzerfreundliche **Weboberfläche** zur Signalverwaltung 
-   Upload eigener Signaldateien über WLAN 
-   Signale im Mikrovoltbereich durch Tiefpassfilter & Spannungsteiler 
-   Keine zusätzliche Softwareinstallation erforderlich (Bedienung im
    Browser)

## Architektur

-   **Hardware**
    -   ESP32-S3 Mikrocontroller (Steuerung, WLAN, Webserver)
    -   DAC8412FPZ (Digital-Analog-Wandler, 4 Kanäle)
    -   TL071CDR Operationsverstärker (aktive Tiefpassfilter)
    -   Spannungsversorgung über USB-C, mit ±5 V und 3,3 V Regulierung
-   **Software**
    -   Frontend: HTML, CSS, JavaScript (Single Page Application)
    -   Backend: ESP32-Firmware in C++ (PlatformIO, ESPAsyncWebServer)
    -   Dateiverwaltung & Signalzuweisung über Webinterface
    -   DAC-Steuerung und Echtzeit-Signalausgabe

## Getting Started

1.  Platine über **USB-C** mit Strom versorgen 
2.  Verbindung mit dem WLAN **EEGsimulator** (Passwort:
    `EEGsimulator2525`) 
3.  Browser öffnen und `192.168.4.1` eingeben 
4.  Über die Weboberfläche:
    -   Signaldateien (`.txt`) hochladen 
    -   Dateien Kanälen zuweisen 
    -   „Verarbeitung anstoßen" und anschließend „Abspielen" klicken

## Dateiformat für Signale

-   Textdateien mit Zahlenwerten (positive/negative Amplituden) 
-   Automatische Skalierung auf den **12-Bit Wertebereich (0--4095)** 
-   Mapping:
    -   Negative Werte → 0--2047 
    -   Positive Werte → 2048--4095

## Hinweise

-   Für stabile Ausgabe eine saubere Stromversorgung sicherstellen 
-   Signale lassen sich per Oszilloskop überwachen 
-   Änderungen an Reihenfolge oder Kanälen erfordern erneute
    Verarbeitung

## Ausblick

-   Verbesserte analoge Filter (z. B. Sallen-Key) 
-   Optimierte Stromversorgung (LDOs oder Akkubetrieb) 
-   Bibliothek vordefinierter EEG-Signalformen (Alpha-/Beta-Wellen
    etc.) 
-   Live-Visualisierung der Signale direkt im Browser

------------------------------------------------------------------------

Entwickelt von **Johannes Fischer** im Rahmen des Projektseminars an
der **Hochschule Wismar**
