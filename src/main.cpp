#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

const char* ssid = "Nevatron";
const char* password = "Nevatron!2024Ok";

#include "sRusun_Firmware_Update.h"

void setup() 
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  
  Serial.println("Hello From Program V2!");
  
  delay(10000);
  performOTA();
}

void loop() 
{

}