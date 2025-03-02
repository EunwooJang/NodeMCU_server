#ifndef GLOBAL_H
#define GLOBAL_H

#define ENABLE_DEBUG // 디버깅용 시리얼 통신 활성화

#ifdef ENABLE_DEBUG
  // 디버그 출력이 활성화된 경우, Serial.print 함수를 그대로 사용
  #define DEBUG_BEGIN(x)     Serial.begin(x);
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
 #else
  // 디버그 출력이 비활성화된 경우, 아무 작업도 하지 않는 매크로로 대체
  #define DEBUG_BEGIN(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// #define ENABLE_EMAIL  // 이메일 기능 활성화

//외부 라이브러리
#include <Arduino.h>
#include <SoftwareSerial.h>

#include <ESPAsyncWebServer.h>
#include <ESP_Mail_Client.h>

#include <Updater.h>

#include <LittleFS.h>
#include <SdFat.h>

// 상태 플래그 변수
extern bool isMeasuring;
extern bool isSaving;
extern bool isUpdating;
extern bool isSDbusy;
extern bool isSDoff;
extern bool isSwitched;

// 센서 종류별 슬레이브 활성화 여부
extern bool alive_temp_slave[];
extern bool alive_mag_slave[];

#endif