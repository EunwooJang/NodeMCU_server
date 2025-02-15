#ifndef WEBSOCKETHANDLER_H
#define WEBSOCKETHANDLER_H

#include "global.h"

// 버전 정보
extern const char *version;

// 온습도 센서 슬레이브 아두이노 개수 및 각 아두이노별 센서 개수
extern int temp_slave_Amount;   // slave 아두이노 개수
extern int temp_sensor_Amount;  // 각 아두이노당 센서 개수


// 자기장 센서 슬레이브 아두이노 개수 및 각 아두이노별 센서 개수
extern int magnetic_slave_Amount;   // slave 아두이노 개수
extern int magnetic_sensor_Amount;  // 각 아두이노당 센서 개수

extern int cur_index;

//데이터 관련 변수(초기 설정)
extern unsigned long acquisitiontimeInterval;

extern bool new_client;
extern int client_n;

extern int alive_temp_slave[];
extern int alive_mag_slave[];

extern int max_counts;

extern SdFat sd;  // SD 카드 객체 전역 변수 선언

// WebSocket 설정 함수
void setupWebSocket(AsyncWebServer &server, AsyncWebSocket &ws);

#endif
