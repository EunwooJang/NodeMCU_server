#ifndef GLOBAL_H
#define GLOBAL_H

//라이브러리
#include <Arduino.h>
#include <SoftwareSerial.h>

#include <time.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <ESP_Mail_Client.h>

#include <ArduinoOTA.h>
#include <Updater.h>

#include <LittleFS.h>
#include <SdFat.h>

// 측정 상태 플래그 변수
extern bool isMeasuring;
extern bool isSaving;
extern bool isUpdating;
extern bool isSDbusy;

extern String currentlysavingFile;
extern unsigned long GMT;

#endif
