#include <ESP8266WiFi.h>
#include "WebSocketHandler.h"

// 버전 정보
const char* version = "1.0.2";  //HTML에 공유


// Slave 아두이노 및 센서 관련 설정
int temp_slave_Amount = 4;  // slave 아두이노 개수
int temp_sensor_Amount = 5;  // 각 아두이노당 센서 개수

int magnetic_slave_Amount = 2;  // slave 아두이노 개수
int magnetic_sensor_Amount = 1;  // 각 아두이노당 센서 개수

int cur_index = 0;

bool new_client = false;

int alive_temp_slave[] = {1, 0, 1, 0};
int alive_mag_slave[] = {1, 0};

// 이상하다고 판단하는 것은 센서의 온습도값이 각각 0,0 인 상태가 5번 연속 -> 센서 이상
// 하나의 아두이노의 모든 센서의 값들이 똑같이 0, 0인 상태가 5번 연속 -> 아두이노 이상 -> 해당 이상하다고 판단한 아두이노,센서 위치의 값을 2로 수정 및 is_Werid = True로 전환하는 함수
// 연속된 이상 상태를 감지하는 카운트 배열

int max_counts = 100;

// 상태 플래그 변수
bool isMeasuring = false;

//데이터 관련 변수(초기 설정)
unsigned long acquisitiontimeInterval = 10000;  // 데이터 수집 주기 (ms). //HTML에 공유

void setupWebSocket(AsyncWebSocket& ws) {
    ws.onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {

        if (type == WS_EVT_CONNECT) {
            new_client = true;
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
                if (message.equalsIgnoreCase("start")) {
                    isMeasuring = true;
                    // Serial.println("측정을 시작합니다.");

                } else if (message.equalsIgnoreCase("stop")) {
                    isMeasuring = false;
                    // Serial.println("측정을 중지합니다.");
                } else if (message.startsWith("set ") && !isMeasuring) {
                    String numberPart = message.substring(4); // "set max event " 이후의 문자열 추출
                    numberPart.trim(); // 앞뒤 공백 제거
                    if (numberPart.length() > 0 && numberPart.toInt() > 0) { // 숫자인지 확인
                        max_counts = numberPart.toInt(); // 숫자로 변환 후 저장
                    } else {
                        Serial.println("Invalid number format for max event.");
                    }
                }

            }
        }
    });
}
