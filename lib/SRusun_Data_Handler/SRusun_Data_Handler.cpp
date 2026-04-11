#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <sys/time.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PZEM004Tv30.h>
#include <stdint.h>
#include "driver/pcnt.h"


#include "sRusun_Data_Handler.h"
#include "SRusun_Base_Function.h"
#include "sRusun_Flowmeter.h"
#include "sRusun_Kwhmeter.h"
#include "sRusun_servo_valve.h"
#include "SRusun_electricity_SSR.h"

String apiKey = "";
String unitId = "";
String flatId = "";
String deviceId = "";


int getAPIKey(bool logger)
{
  HTTPClient https;
  https.begin(url_config);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> docgetAPIKey;
  docgetAPIKey["device_serial_number"] = deviceSerialNumber;
  docgetAPIKey["device_mac_address"] = deviceMacAddress;

  String jsonBodyGetAPIkey;
  serializeJsonPretty(docgetAPIKey, jsonBodyGetAPIkey);

  //1A
  if (logger) 
  {
    String jsonBodyLogGetAPIKey;
    serializeJsonPretty(docgetAPIKey, jsonBodyLogGetAPIKey);
    logMsg("GetAPIKey", "Requesting API key with payload: " + jsonBodyLogGetAPIKey, logger);
  }

  int httpResponseCode = https.POST(jsonBodyGetAPIkey);

  if (httpResponseCode == 200)
  {
    String response = https.getString();
    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    //Jika error
    if (error) 
    {
      logMsg("GetAPIKey", "Failed to parse response: " + String(error.c_str()), enableLogging);
      https.end();
      return httpResponseCode;
    }

    //Jika berhasil 1B
    JsonObject data = responseDoc["data"];

    if(!data.isNull())
    {
      apiKey = data["device"]["device_api_key"].as<String>();
      unitId = data["device"]["unit_id"].as<String>();
      flatId = data["device"]["flat_id"].as<String>();
      deviceId = data["device"]["device_id"].as<String>();
      if (logger)
      {
        logMsg("GetAPIKey", "Successfully fetched API key and device info:", logger);
        logMsg("GetAPIKey", "API Key: " + apiKey, logger);
        logMsg("GetAPIKey", "Unit ID: " + unitId, logger);
        logMsg("GetAPIKey", "Flat ID: " + flatId, logger);
        logMsg("GetAPIKey", "Device ID: " + deviceId, logger);
      }
      https.end();
      return httpResponseCode;
    }

    else
    {
      logMsg("GetAPIKey", "Response data is null", enableLogging);
      https.end();
      return httpResponseCode;
    }

    https.end();
    return httpResponseCode;
  }

  else 
  {
    logMsg("GetAPIKey", "Failed to fetch API key with HTTP code: " + String(httpResponseCode), enableLogging);
    https.end();
    return httpResponseCode;
  }
}

String deviceToken = "";

int authDevice(bool logger)
{
  HTTPClient https;
  https.begin(url_auth);
  https.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> docAuth;
  docAuth["device_mac_address"] = deviceMacAddress;
  docAuth["device_serial_number"] = deviceSerialNumber;
  docAuth["device_api_key"] = apiKey;

  String jsonBodyAuth;
  serializeJson(docAuth, jsonBodyAuth);


  //2A
  if (logger) 
  {
    String jsonBodyLogAuth;
    serializeJsonPretty(docAuth, jsonBodyLogAuth);
    logMsg("AuthDevice", "Authenticating device with payload: " + jsonBodyLogAuth, logger);
  }

  int responseCode = https.POST(jsonBodyAuth);

  if (responseCode == 200) 
  {
    String response = https.getString();
    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    //Jika Error
    if (error) 
    {
      logMsg("AuthDevice", "Failed to parse response: " + String(error.c_str()), enableLogging);
      https.end();
      return responseCode;
    }

    //Jika Berhasil 2B
    if (logger)
    {
      String jsonResponseLog;
      serializeJsonPretty(responseDoc, jsonResponseLog);
      logMsg("AuthDevice", "Authentication successful. Server response: " + jsonResponseLog, logger);
    }

    if (!responseDoc["data"]["token"].isNull())
    {
      deviceToken = responseDoc["data"]["token"].as<String>();
      
      if (logger)
      {
        logMsg("AuthDevice", "Device token retrieved: " + deviceToken, logger);
      }

      https.end();
      return responseCode;
    }
    else
    {
      logMsg("AuthDevice", "Token not found in response", enableLogging);
      https.end();
      return responseCode;
    }
  }

  else 
  {
    logMsg("AuthDevice", "Authentication failed with HTTP code: " + String(responseCode), enableLogging);
    https.end();
    return responseCode;
  }
}

//========================== Kirim data ke Server dan terima perintah

StaticJsonDocument<1024> composeData(bool logger)
{
  StaticJsonDocument<1024> doc;

  doc["device_id"] = deviceId;
  doc["flat_id"] = flatId;
  doc["unit_id"] = unitId;
  doc["device_token"] = deviceToken;
  doc["device_mac_address"] = deviceMacAddress;
  doc["timestamp"] =  ((uint64_t)timestamp)*1000;

  JsonArray result = doc.createNestedArray("device_components_result");

  // Aktuator: water_valve
  JsonObject actWater = result.createNestedObject();
  actWater["component_name"] = "water_valve";
  actWater["component_type"] = "actuator";
  actWater["value"]["status"] = water_valve_state;
  actWater["value"]["angle"] = currAngle;

  // Sensor: water_flowmeter
  JsonObject sensorWater = result.createNestedObject();
  sensorWater["component_name"] = "water_flowmeter";
  sensorWater["component_type"] = "sensor";
  sensorWater["value"]["volume"] = volume;
  sensorWater["value"]["flowRate"] = flowRate;

  // Aktuator: SSR
  JsonObject actElec = result.createNestedObject();
  actElec["component_name"] = "electricity";
  actElec["component_type"] = "actuator";
  actElec["value"]["status"] = SSR_state;


  // Sensor: electricity
  JsonObject sensorElec = result.createNestedObject();
  sensorElec["component_name"] = "electricity";
  sensorElec["component_type"] = "sensor";
  sensorElec["value"]["total_energy"] = energy;
  sensorElec["value"]["power"] = power;
  sensorElec["value"]["voltage"] = voltage;

  return doc;
}

int sendData(StaticJsonDocument<1024> doc, bool logger)
{
  String jsonBody;
  serializeJsonPretty(doc, jsonBody);

  //3A
  if (logger)
  {
    logMsg("SendData", "Sending data to server with payload: " + jsonBody, logger);
  }
  
  HTTPClient https;
  https.begin(url_data);
  https.addHeader("Content-Type", "application/json");

  int responseCode = https.POST(jsonBody);

  if (responseCode == 200)
  {
    String response = https.getString();

    StaticJsonDocument <1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    //Jika error
    if (error) 
    {
      logMsg("SendData", "Failed to parse response: " + String(error.c_str()), enableLogging);
      https.end();
      return responseCode;
    }

    //Jika berhasil 3B - Handle servo control instruction
    if (!error && responseDoc["data"]["user_instruction"].is<JsonObject>())
    {
      JsonObject instruction = responseDoc["data"]["user_instruction"];
      String componentName = instruction["component_name"].as<String>();
      bool instructionFrom = instruction["instruction_from"].as<bool>();
      bool instructionTo = instruction["instruction_to"].as<bool>();

      if (logger)
      {
        String jsonResponseLog;
        serializeJsonPretty(responseDoc, jsonResponseLog);
        logMsg("SendData", "Data sent successfully. Server response: " + jsonResponseLog, logger);
      }

      logMsg("1. sendData():", "Received instruction for component: " + componentName, logger);

      if (componentName.equals("water_valve"))
      {
        logMsg("2. sendData():", "Instruction is for water_valve. Current state: " + String(water_valve_state), logger);
        logMsg("3. sendData():", "Instruction received - Change valve to: " + String(instructionTo ? "OPEN" : "CLOSE"), logger);
        
        
        if (instructionTo)
        {
          logMsg("4. sendData():", "Instruction is to OPEN the valve. Executing ServoValveOpen().", logger);
          ServoValveOpen();
        }
        else
        {
          logMsg("4. sendData():", "Instruction is to CLOSE the valve. Executing ServoValveClose().", logger);
          ServoValveClose();
        }
      }

      //SSR Handler
      if(componentName.equals("electricity"))
      {
        logMsg("2. sendData():", "Instruction is for SSR. Current state: " + String(SSR_state), logger);
        logMsg("3. sendData():", "Instruction received - Change SSR to: " + String(instructionTo ? "ON" : "OFF"), logger);

        
        if(instructionTo)
        {
          logMsg("4. sendData():", "Instruction is to TURN ON the SSR. Executing SSROpen().", logger);
          SSROpen();
        }
        else
        {
          logMsg("4. sendData():", "Instruction is to TURN OFF the SSR. Executing SSRClose().", logger);
          SSRClose();
        }
      }

    }

    https.end();
    return responseCode;
  }

  else
  {
    logMsg("sendData():", "Failed to send data with HTTP code: " + String(responseCode), logger);
    https.end();
    return responseCode;
  }
 }

bool saveBackup(String backupFileName, StaticJsonDocument<1024> doc, bool logger)
{
  logMsg("saveBackup()", "Saving backup data to SD card with filename: " + backupFileName, logger);
  File file = SD.open("/backup/" + backupFileName, FILE_APPEND);

  if (!file) 
  {
    logMsg("SaveBackup", "Failed to open backup.json for writing!", enableLogging);
    return false;
  }

  //Serial.println("File size before append: " + String(file.size()) + " bytes");

  //Concate BackFileName.json docAppend
  String appendJson;
  serializeJson(doc, appendJson);
  file.println(appendJson);

  file.flush();  // Force write to SD card
  vTaskDelay(10 / portTICK_PERIOD_MS); // Give SD time to write

  //Serial.println("File size after append: " + String(file.size()) + " bytes");

  
  file.close();
  vTaskDelay(10 / portTICK_PERIOD_MS);  // Give SD time after close

  if (logger)
  {
    String jsonBody;
    serializeJsonPretty(doc, jsonBody);
    logMsg("SaveBackup", "Successfully wrote to backup.json. Success... closed. " + jsonBody, logger);
  }

  return true;
}

void showBackup(String backupFileName)
{
  File file = SD.open("/backup/" + backupFileName, FILE_READ);

  logMsg("ShowBackup", "Contents of " + backupFileName, enableLogging);
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    logMsg("ShowBackup", line, enableLogging);
  }
  
  file.close();
}

String composeBatchData(String batchFileName, bool logger)
{
  File file = SD.open("/backup/" + batchFileName, FILE_READ);

  String batchArray;

  // Read each line (JSON object) from the batch file and add to the 
  if (file.available())
  {
    
    batchArray = "[" + file.readStringUntil('\n');

  }

  while (file.available())
  {
    //Serial.println("composeBatchData(): Reading line from batch file...");
    String line = file.readStringUntil('\n');
    if (line.length() > 0) 
    {
      batchArray += "," + line;
      //Serial.println("composeBatchData(): Successfully parsed line into JSON object.");      
    }
  }
  
  batchArray += "]";
  file.close();
  
  return batchArray;
}

int sendBatch(String batchArray, bool logger)
{
  HTTPClient https;
  https.begin(url_batch);
  https.addHeader("Content-Type", "application/json");
  
  DynamicJsonDocument docBatch(4096);
  docBatch["device_token"] = deviceToken;
  docBatch["device_mac_address"] = deviceMacAddress;

  JsonArray result = docBatch.createNestedArray("sensing_data");

  // Parse the batch array string into JSON objects and add to the batch document
  DeserializationError error = deserializeJson(docBatch["sensing_data"], batchArray);
  if (error) 
  {
    logMsg("SendBatch", "Failed to parse batch array: " + String(error.c_str()), enableLogging);
    https.end();
    return 999; // Custom error code for batch parsing failure
  }

  String jsonBody;
  serializeJsonPretty(docBatch, jsonBody);

  //4A
  if (logger)
  {
    logMsg("SendBatch", "Sending batch data to server with payload: " + jsonBody, logger);
  }

  int responseCode = https.POST(jsonBody);

  if (responseCode == 200)
  {
    String response = https.getString();

    StaticJsonDocument <1024> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    //Jika error
    if (error) 
    {
      logMsg("SendBatch", "Failed to parse response: " + String(error.c_str()), enableLogging);
      https.end();
      return responseCode;
    }

    //Jika berhasil 4B
    if (logger)
    {
      String jsonResponseLog;
      serializeJsonPretty(responseDoc, jsonResponseLog);
      logMsg("SendBatch", "Batch data sent successfully. Server response: " + jsonResponseLog, logger);
    }

    https.end();
    return responseCode;
  }

  else
  {
    logMsg("SendBatch", "Failed to send batch data with HTTP code: " + String(responseCode), enableLogging);
    https.end();
    return responseCode;
  }

}

