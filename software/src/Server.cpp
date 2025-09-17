#include "Server.hpp"
#include <WiFi.h>
#include <esp_event.h>
#include <esp_netif.h>

int ausgabeFrequenzHz = 100; // Default

void ladeFrequenzAusDatei() {
    File f = SPIFFS.open("/freq.cfg", "r");
    if (f) {
        String val = f.readStringUntil('\n');
        int hz = val.toInt();
        if (hz >= 1 && hz <= 1000) ausgabeFrequenzHz = hz;
        f.close();
    }
}

void speichereFrequenzInDatei(int hz) {
    File f = SPIFFS.open("/freq.cfg", "w");
    if (f) {
        f.println(hz);
        f.close();
    }
}

void setupRoutes(AsyncWebServer& server) {
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"uploading\"}");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!filename.endsWith(".txt")) return;

        static File uploadFile;
        if (index == 0) {
            uploadFile = SPIFFS.open("/" + filename, "w");
        }
        if (uploadFile) uploadFile.write(data, len);
        if (final && uploadFile) uploadFile.close();
    });
}
  
  std::vector<float> extractNumbersRegex(const String &content) {
    std::vector<float> numbers;
    std::string s = content.c_str();
    std::regex floatRegex("-?[0-9]+(?:\\.[0-9]+)?");
    auto numbersBegin = std::sregex_iterator(s.begin(), s.end(), floatRegex);
    auto numbersEnd = std::sregex_iterator();
    for (std::sregex_iterator i = numbersBegin; i != numbersEnd; ++i) {
  std::smatch match = *i;
  float value = atof(match.str().c_str());
  numbers.push_back(value);
    }
    return numbers;
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
      if (!first) filesList += ",";
      String name = String(file.name());
      if (name.startsWith("/")) name = name.substring(1);
      filesList += "\"" + name + "\"";
      first = false;
      file = root.openNextFile();
    }
    filesList += "]}";
    return filesList;
  }
  
  void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (SPIFFS.exists("/HTML_Server.html")) {
        request->send(SPIFFS, "/HTML_Server.html", "text/html");
      } else {
        request->send(404, "text/plain", "Fehler: HTML_Server.html nicht gefunden.");
      }
    });
  
    server.onNotFound([](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
  
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(204, "text/html", "");
    });
  
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/");
    });
  
    server.on("/HS-Wismar_Logo-FIW_V1_RGB.png", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (SPIFFS.exists("/HS-Wismar_Logo-FIW_V1_RGB.png")) {
        request->send(SPIFFS, "/HS-Wismar_Logo-FIW_V1_RGB.png", "image/png");
      } else {
        request->send(404, "text/plain", "Fehler: Logo nicht gefunden.");
      }
    });
  
    server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
      size_t totalBytes = SPIFFS.totalBytes();
      size_t usedBytes = SPIFFS.usedBytes();
      String json = "{";
      json += "\"total\": " + String(totalBytes) + ",";
      json += "\"used\": " + String(usedBytes);
      json += "}";
      request->send(200, "application/json", json);
    });
  
    server.on("/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Upload erfolgreich");
      },
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
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
        if (activeUploads.find(request) != activeUploads.end()) {
          activeUploads[request].file.write(data, len);
        }
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
  
    server.on("/getFiles", HTTP_GET, [](AsyncWebServerRequest *request) {
      String filesList = getUploadedFilesList();
      request->send(200, "application/json", filesList);
    });
  
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
  
    server.on("/processFiles", HTTP_POST, [](AsyncWebServerRequest *request) {
      String channelsJson = "";
      if (request->hasParam("channels", true)) {
        channelsJson = request->getParam("channels", true)->value();
      }
  
      StaticJsonDocument<2048> resultDoc;
      JsonArray results = resultDoc["results"].to<JsonArray>();
  
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, channelsJson);
      if (error) {
        request->send(400, "text/plain", "Fehler beim Parsen des channels JSON.");
        return;
      }
  
      JsonArray channelsArray = doc.as<JsonArray>();
      // Map zum Sammeln aller Zahlen pro Kanal in Reihenfolge
      std::map<String, std::vector<float>> tempKanalDaten;
  
      for (JsonObject elem : channelsArray) {
        const char* name = elem["name"];
        const char* channel = elem["channel"];
        String filePath = "/" + String(name);
  
        JsonObject res = results.add<JsonObject>();
        res["filename"] = String(name);
        res["channel"] = channel;
  
        if (SPIFFS.exists(filePath)) {
          File file = SPIFFS.open(filePath, "r");
          if (file) {
            String content;
            while (file.available()) content += (char)file.read();
            file.close();
  
            std::vector<float> numbers = extractNumbersRegex(content);
  
            // Werte anhängen, nicht überschreiben!
            if (!numbers.empty()) {
              auto& vec = tempKanalDaten[String(channel)];
              vec.insert(vec.end(), numbers.begin(), numbers.end());
            }
  
            if (content.length() == 0 || numbers.empty()) {
              res["selfCheck"] = "Keine Zahlen gefunden. Datei übersprungen.";
              res["error"] = "Keine Zahlen erkannt";
              continue;
            }
  
            JsonArray nums = res["numbers"].to<JsonArray>();
            for (float n : numbers) nums.add(n);
  
            res["numberCount"] = (int)numbers.size();
            res["selfCheck"] = "OK";
          } else {
            res["error"] = "Fehler beim Öffnen der Datei.";
            res["selfCheck"] = "Fehler";
          }
        } else {
          res["error"] = "Datei nicht gefunden.";
          res["selfCheck"] = "Fehler";
        }
      }
  
      // Nach dem Durchlauf: tempKanalDaten in kanalDaten übernehmen
      kanalDaten = std::move(tempKanalDaten);
  
      String response;
      serializeJson(resultDoc, response);
      request->send(200, "application/json", response);
    });

    server.on("/play", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (kanalDaten.empty()) {
        request->send(400, "text/plain", "❌ Keine Kanaldaten geladen. Bitte zuerst Datei hochladen und /processFiles aufrufen.");
        return;
    }
    startAbspielTask();
    request->send(200, "text/plain", "Wiedergabe gestartet");
});


    server.serveStatic("/script.js", SPIFFS, "/script.js");
  
    server.on("/resetChannels", HTTP_POST, [](AsyncWebServerRequest *request) {
        kanalDaten.clear();
        // Optional: weitere Arrays zurücksetzen, falls benötigt
        // uploadedFiles.clear();
        // activeUploads.clear();
        request->send(200, "text/plain", "Kanaldaten zurückgesetzt");
    });
  
    server.on("/getFrequency", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"frequency\":" + String(ausgabeFrequenzHz) + "}");
    });

    server.on("/setFrequency", HTTP_POST, [](AsyncWebServerRequest *request){
        // This handler is required but not used for body data
    }, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body((char*)data, len);
        if (body.length() == 0) {
            request->send(400, "text/plain", "Kein JSON empfangen");
            request->send(400, "text/plain", "Kein JSON empfangen");
            return;
        }
        StaticJsonDocument<64> doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            request->send(400, "text/plain", "Ungültiges JSON: " + String(err.c_str()));
            return;
        }
        if (!doc.containsKey("frequency")) {
            request->send(400, "text/plain", "Feld 'frequency' fehlt");
            return;
        }
        int freq = doc["frequency"];
        if (freq >= 1 && freq <= 1000) {
            ausgabeFrequenzHz = freq;
            speichereFrequenzInDatei(freq);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Ungültige Frequenz");
        }
    }
);

    server.begin();
    Serial.println("Webserver gestartet.");
  }