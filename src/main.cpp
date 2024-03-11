#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "ModbusIP_ESP8266.h"
#include <EEPROM.h>

const char* ssid = "ESP32-Ahmet2";
const char* password = "123456789";
boolean temp = false;
ModbusIP mb;

const int ledPin = 23;
const int ledPin2 = 4;

String readEEPROMString(int start, int maxLen) {
  String result;
  for (int i = start; i < start + maxLen; ++i) {
    char c = EEPROM.read(i);
    if (c == '\0') break; 
    result += c;
  }
  return result;
}

void clearEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void writeEEPROMString(int start, String data) {
  int i;
  for (i = 0; i < data.length(); i++) {
    EEPROM.write(start + i, data[i]);
  }
  EEPROM.write(start + i, '\0');
  EEPROM.commit();
}

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT); 

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(ssid, password);
  Serial.println("SoftAP modunda WiFi ağı başlatıldı.");

  Serial.println(WiFi.softAPIP());


  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    WiFi.scanNetworks(true);
    request->send(200, "application/json", "{\"message\":\"Tarama basladi.\",\"status\":\"true\"}");
  });


  server.on("/wifi/list", HTTP_GET, [](AsyncWebServerRequest *request) {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_FAILED) {
          request->send(500, "application/json", "{\"error\":\"Tarama basarısız oldu.\",\"status\":\"false\"}");
          return;
      } else if (n == WIFI_SCAN_RUNNING) {
          request->send(200, "application/json", "{\"message\":\"Tarama devam ediyor.\",\"status\":\"wait\"}");
          return;
      }

      DynamicJsonDocument doc(1024);
      JsonArray networks = doc.to<JsonArray>();

      for (int i = 0; i < n; ++i) {
          JsonObject network = networks.createNestedObject();
          network["ssid"] = WiFi.SSID(i);
      }

      WiFi.scanDelete();

      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", "{\"message\":\"Tarama bitti\",\"status\":\"true\",\"data\":" + response + "}" );
  });

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, data);
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];

    Serial.print("Bağlanılan SSID: ");
    Serial.println(ssid);
    Serial.print("Şifre: ");
    Serial.println(password);

    WiFi.begin(ssid, password);

    long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
      delay(100);
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
      request->send(500, "application/json", "{\"error\":\"Baglantı basarısız oldu.\",\"status\":\"false\"}");
    } else {
      Serial.println("Bağlandı!");
      Serial.print(WiFi.localIP().toString());
      clearEEPROM();
      writeEEPROMString(0, ssid);
      int passwordStart = strlen(ssid) + 1;
      writeEEPROMString(passwordStart, password);
      request->send(200, "application/json", "{\"status\": \"true\",\"message\": \"Baglantı Basarılı.\",\"ip\": \"" + WiFi.localIP().toString() + "\"}");
      delay(1000);
      WiFi.mode(WIFI_MODE_STA);
    }
  });

  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
      clearEEPROM();
      Serial.println("Saved network credentials deleted. Device now in AP+STA mode.");
      request->send(200, "application/json", "{\"status\":\"true\",\"message\":\"Network credentials deleted, device reset to AP+STA mode.\"}");
      WiFi.disconnect(); 
      WiFi.mode(WIFI_MODE_APSTA);
  });

  server.begin();

  mb.server();
  mb.addHreg(1); 
  mb.addHreg(2); 

}

void loop() {
  if(temp == false) {
    if (WiFi.status() != WL_CONNECTED) {
      
      String ssid = readEEPROMString(0, 32); 
      int passwordStart = ssid.length() + 1; 
      String password = readEEPROMString(passwordStart, 64 - passwordStart); 
      
      if (ssid.length() > 0 && password.length() > 0) {
        
        WiFi.begin(ssid, password);
        
        unsigned long startAttemptTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
          delay(100); 
        }

        if (WiFi.status() == WL_CONNECTED) {
          WiFi.mode(WIFI_MODE_STA); 
          Serial.println("WiFi bağlantısı başarılı!");
          Serial.println(WiFi.status());
        }
      }
      temp = true;
    }
  } else {
    
    mb.task();
    delay(10);

    digitalWrite(ledPin, mb.Hreg(1) ? HIGH : LOW);
    digitalWrite(ledPin2, mb.Hreg(2) ? HIGH : LOW);
  }
}

