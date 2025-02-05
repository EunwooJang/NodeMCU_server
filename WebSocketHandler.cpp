#include <ESP8266WiFi.h>
#include "WebSocketHandler.h"

// 버전 정보
const char* version = "1.0.0";  //HTML에 공유


// Slave 아두이노 및 센서 관련 설정
const int temp_slave_Amount = 5;  // slave 아두이노 개수
const int temp_sensor_Amount = 5;  // 각 아두이노당 센서 개수

const int magnetic_slave_Amount = 4;  // slave 아두이노 개수
const int mangetic_sensor_Amount = 1;  // 각 아두이노당 센서 개수


// 상태 플래그 변수
bool isMeasuring = false;

//데이터 관련 변수(초기 설정)
const unsigned long acquisitiontimeInterval = 10000;  // 데이터 수집 주기 (ms). //HTML에 공유
const int defaultEventNumber = 100;  // 기본 이벤트 수. //HTML에 공유
int adjustedEventNumber = 100;  // SD 카드 용량을 고려한 조정된 이벤트 수. //HTML에 공유


void setupWebSocket(AsyncWebSocket& ws) {
    ws.onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.println("WebSocket Client Connected");

        } else if (type == WS_EVT_DISCONNECT) {
            Serial.println("WebSocket Client Disconnected");
        
        // 서버에서 받은 명령어에 대한 처리
        } else if (type == WS_EVT_DATA) {
            // 메시지가 수신될 때 처리
            AwsFrameInfo* info = (AwsFrameInfo*)arg;

            // 텍스트 메시지 처리
            if (info->opcode == WS_TEXT) {
                String message = "";
                for (size_t i = 0; i < len; i++) {
                    message += (char)data[i];
                }
                Serial.printf("Received Text Message: %s\n", message.c_str());
                // 명령어 처리
                if (message.equalsIgnoreCase("measure")) {
                    isMeasuring = true;
                    Serial.println("측정을 시작합니다.");

                } else if (message.equalsIgnoreCase("stop")) {
                    isMeasuring = false;
                    Serial.println("측정을 중지합니다.");
                }
            }
        }
    });
}
