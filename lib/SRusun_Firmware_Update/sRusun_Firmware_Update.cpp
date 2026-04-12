#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#include "sRusun_Firmware_Update.h"

char* firmware_url = "https://github.com/MedikaSurya/ESP32_Firmware_OTA_Update/raw/main/.pio/build/denky32/firmware.bin";
String curr_version = "1.0.0";
String recieve_version = "";
String whenToUpdate = "now";
bool is_updating = false;

void performOTA() 
{
  Serial.println("Starting OTA Update from GitHub...");
  
  WiFiClientSecure client;
  // Setting insecure skips SSL certificate validation. 
  // For production, you should provide the GitHub Root CA certificate instead.
  client.setInsecure(); 

  HTTPClient http;
  http.begin(client, firmware_url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) 
  {
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);

    // Check if there is enough space to begin OTA
    bool canBegin = Update.begin(contentLength);
    if (canBegin) 
    {
      Serial.println("Writing firmware to ESP32...");
      size_t written = Update.writeStream(http.getStream());

      if (written == contentLength) 
      {
        Serial.println("Written successfully!");
        if (Update.end()) 
        {
          Serial.println("OTA Complete. Restarting...");
          is_updating = false; // Reset updating flag
          
          ESP.restart(); // Restart into the new firmware


        } else 
        {
          Serial.printf("OTA End Failed: %s\n", Update.errorString());
        }

      }

      else 
      {
        Serial.println("Written bytes do not match content length.");
      }
    }

    else 
    {
      Serial.println("Not enough space to begin OTA.");
    }
  }
   
  else 
  {
    Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
  }
  http.end();
}
