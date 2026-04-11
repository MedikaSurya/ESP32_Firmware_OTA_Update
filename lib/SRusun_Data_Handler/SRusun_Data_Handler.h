#ifndef SRUSUN_DATA_HANDLER_H
#define SRUSUN_DATA_HANDLER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern String apiKey;
extern String unitId;
extern String flatId;
extern String deviceId;

int getAPIKey(bool logger);
int authDevice(bool logger);

StaticJsonDocument<1024> composeData(bool logger);

int sendData(StaticJsonDocument<1024> doc, bool logger);

bool saveBackup(String backupFileName, StaticJsonDocument<1024> doc, bool logger);

void showBackup(String backupFileName);

String composeBatchData(String batchFileName, bool logger);

int sendBatch(String batchArray, bool logger);

#endif // SRUSUN_DATA_HANDLER_H
