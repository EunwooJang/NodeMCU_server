#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

#include <FS.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

#include "WifiConnect.h"

#include "WebSocketHandler.h"
#include "SetupHandler.h"
#include "OTAUpdaterHandler.h"

#include "DHT_multi.h"
#include <SoftwareSerial.h>


SoftwareSerial hc12(D2, D1); // HC-12 모듈을 위한 SoftwareSerial 객체

// DHTMulti 객체 생성
DHTMulti dhtMulti(temp_slave_Amount);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

OTAUpdateHandler otaHandler(&server);  // OTA 핸들러 생성


SdFat sd; // SD 카드 객체

WiFiUDP ntpUDP; // NTP를 위한 UDP 객체 생성
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC 시간 동기화, 1분 간격 갱신

void setup() {
  Serial.begin(115200);
  hc12.begin(9600); // HC-12 통신 시작 (메인 함수에서 설정)

  // WifiConnect 객체 생성
  WifiConnect wifi;
  wifi.connect();  // WiFi 연결 시도

  // SPIFFS 파일 시스템 초기화
  if (!SPIFFS.begin()) {
      Serial.println("SPIFFS Mount Failed");
      return;
  }

  // SD 파일 시스템 초기화
  if (!sd.begin(15, SD_SCK_MHZ(25))) {
      Serial.println("SD card initialization failed!");
      return;
  }
  
  setupWebSocket(ws);
  server.addHandler(&ws);
  
  // Setup 관련 핸들러 설정
  handleSetupRoutes(server);

  otaHandler.setup();

  // NTPClient 시작
  timeClient.begin();
  while (!timeClient.update()) {
  }

  // 메인 페이지 설정
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // favicon 설정 빈킨
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/favicon.ico")) {
      request->send(SPIFFS, "/favicon.ico", "image/x-icon");
    } else {
      request->send(204); // No Content 응답
    }
  });

  // 서버 시작
  server.begin();
  Serial.println("HTTP server started");
}

String currentFileName = ""; // 현재 데이터 저장 파일 이름
int switched = 0;

// Base64 인코딩을 수행하는 함수
String base64Encode(const uint8_t *data, size_t length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encodedString = "";

    for (size_t i = 0; i < length; i += 3) {
        uint32_t buffer = (data[i] << 16);
        if (i + 1 < length) buffer |= (data[i + 1] << 8);
        if (i + 2 < length) buffer |= data[i + 2];

        encodedString += base64_chars[(buffer >> 18) & 0x3F];
        encodedString += base64_chars[(buffer >> 12) & 0x3F];
        encodedString += (i + 1 < length) ? base64_chars[(buffer >> 6) & 0x3F] : '=';
        encodedString += (i + 2 < length) ? base64_chars[buffer & 0x3F] : '=';
    }

    return encodedString;
}

// 데이터를 Base64로 인코딩 후 SD 카드에 저장하는 함수
void saveDataToSD(unsigned long unixTime, const char *result) {
    // 파일 이름 설정
    if (currentFileName == "") {
        currentFileName = String(unixTime) + ".txt";
        Serial.println("새 파일 생성: " + currentFileName);
    }

    // 104바이트 데이터 생성 (4바이트 unixTime + 100바이트 result)
    uint8_t rawData[104] = {0};
    memcpy(rawData, &unixTime, 4);  // UnixTime 4바이트 저장
    memcpy(rawData + 4, result, 100); // result 100바이트 저장

    // Base64 인코딩 수행
    String encodedData = base64Encode(rawData, 104);

    // SD 카드에 데이터 저장
    File32 file = sd.open(currentFileName.c_str(), O_RDWR | O_CREAT | O_APPEND);
    if (!file) {
        Serial.println("파일 열기 실패");
        return;
    }

    file.println(encodedData); // Base64 인코딩된 데이터 저장
    file.close();

    // Serial.println("데이터 저장 완료 (Base64 인코딩): " + encodedData);
}

void loop() {

  ws.cleanupClients();
  
  // WebSocket으로 주기적으로 데이터를 전송
  static unsigned long lastWebSocketSendTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastWebSocketSendTime > acquisitiontimeInterval) {
    
    char* data = dhtMulti.getAllSensorData();
    
    // 현재 Unix 시간 가져오기
    unsigned long unixTime = timeClient.getEpochTime(); //수정

    // JSON 형식으로 변환할 데이터를 담을 String 객체 생성
    String payload = "{\"time\":" + String(unixTime) + ",\"result\":[";
    for (int i = 0; i < temp_slave_Amount * 20; i += 2) {  
        uint16_t value = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8); // 리틀 엔디언 변환
        payload += String(value);
        
        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < temp_slave_Amount * 20) {
            payload += ",";
        }
    }
    payload += "]}";

    ws.textAll(payload);

    // Serial.println(payload);

    if (isMeasuring) {
        saveDataToSD(unixTime, data);
        switched = 1;
      }else if (!isMeasuring && switched == 1){
        currentFileName = "";
        switched = 0;
      }

    // 마지막 전송 시간 갱신
    lastWebSocketSendTime = currentTime;

    yield();
  
  }
}


