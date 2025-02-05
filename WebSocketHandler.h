#ifndef WEBSOCKETHANDLER_H
#define WEBSOCKETHANDLER_H

#include <ESPAsyncWebServer.h>

// 버전 정보
extern const char* version;


// 온습도 센서 슬레이브 아두이노 개수 및 각 아두이노별 센서 개수
extern const int temp_slave_Amount;  // slave 아두이노 개수
extern const int temp_sensor_Amount;  // 각 아두이노당 센서 개수

// 자기장 센서 슬레이브 아두이노 개수 및 각 아두이노별 센서 개수
extern const int magnetic_slave_Amount;  // slave 아두이노 개수
extern const int mangetic_sensor_Amount;  // 각 아두이노당 센서 개수


// 상태 플래그 변수
extern bool isMeasuring;


//데이터 관련 변수(초기 설정)
extern const unsigned long acquisitiontimeInterval;
extern const int defaultEventNumber;
extern int adjustedEventNumber;


// WebSocket 설정 함수
void setupWebSocket(AsyncWebSocket& ws);

#endif
