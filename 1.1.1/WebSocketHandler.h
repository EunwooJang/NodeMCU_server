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

// 저장 중일 시, 현재 파일에서의 데이터 인덱스
extern int cur_index;

// 샘플링 긴격
extern unsigned long acquisitiontimeIntervalmillis;

// 접속한 클라이언트 수
extern int client_n;

// 파일 당 최대로 저장할 데이터 수
extern int max_counts;

// 현재 저장 중인 파일 이름
extern String currentlysavingFile;

// 웹서버에서 사용할 SdFat 객체. 이는 메인에서 선언됨
extern SdFat sd;

// WebSocket 설정 함수
void setupWebSocket(AsyncWebServer &server, AsyncWebSocket &ws);

#endif
