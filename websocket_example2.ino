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
#include "qmc5883l_multi.h"
#include <SoftwareSerial.h>


SoftwareSerial hc12(D2, D1); // HC-12 모듈을 위한 SoftwareSerial 객체

// DHTMulti 객체 생성
DHTMulti dhtMulti(temp_slave_Amount, temp_sensor_Amount, alive_temp_slave);

// QMC5883LMulti 객체 생성
QMC5883LMulti compassMulti(magnetic_slave_Amount, magnetic_sensor_Amount, alive_mag_slave);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

OTAUpdateHandler otaHandler(&server);  // OTA 핸들러 생성


SdFat sd; // SD 카드 객체

WiFiUDP ntpUDP; // NTP를 위한 UDP 객체 생성
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC 시간 동기화, 1분 간격 갱신

String cur_payload;
String cur_status;

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

// WebSocket을 통해 최신 상태를 전송하는 함수
String sendStatus() {
    StaticJsonDocument<256> doc; // JSON 문서 객체

    // 최신 변수 값을 JSON에 담기
    doc["version"] = version;
    
    JsonObject temp = doc.createNestedObject("temp");
    temp["slave_Amount"] = temp_slave_Amount;
    temp["sensor_Amount"] = temp_sensor_Amount;
    // alive_temp_slave를 JSON 배열로 추가

    JsonArray temp_slaves = temp.createNestedArray("alive_temp_slave");
    for (int i = 0; i < temp_slave_Amount; i++) {
        temp_slaves.add(alive_temp_slave[i]);
    }

    JsonObject magnetic = doc.createNestedObject("magnetic");
    magnetic["slave_Amount"] = magnetic_slave_Amount;
    magnetic["sensor_Amount"] = magnetic_sensor_Amount;
    
    // alive_mag_slave를 JSON 배열로 추가
    JsonArray mag_slaves = magnetic.createNestedArray("alive_mag_slave");
    for (int i = 0; i < magnetic_slave_Amount; i++) {
        mag_slaves.add(alive_mag_slave[i]);
    }

    doc["isMeasuring"] = isMeasuring;
    doc["cur_index"] = cur_index;

    JsonObject data = doc.createNestedObject("data");
    data["acquisitiontimeInterval"] = acquisitiontimeInterval;
    data["max_events"] = max_counts;

    // JSON을 문자열로 변환 후 WebSocket 전송
    String status;
    serializeJson(doc, status);
    ws.textAll(status);
  
  return status;
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
void saveDataToSD(unsigned long unixTime, const char *result, size_t t_len) {
    // 파일 이름 설정
    if (currentFileName == "") {
        currentFileName = String(unixTime) + 
        "_"  + String(temp_slave_Amount) +
        "_"  + String(temp_sensor_Amount) +
        "_"  + String(magnetic_slave_Amount) +
        "_"  + String(magnetic_sensor_Amount) + ".txt";

        Serial.println("새 파일 생성: " + currentFileName);
    }

    // 104바이트 데이터 생성 (4바이트 unixTime + 100바이트 result)
    uint8_t rawData[4 + t_len] = {0};
    memcpy(rawData, &unixTime, 4);  // UnixTime 4바이트 저장
    memcpy(rawData + 4, result, t_len); // result 100바이트 저장

    // Base64 인코딩 수행
    String encodedData = base64Encode(rawData, 4 + t_len);

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

size_t len1 = 4 * temp_slave_Amount * temp_sensor_Amount;  // dhtMulti 데이터 크기 (예제에서는 96바이트라고 가정)
size_t len2 = 6 * magnetic_slave_Amount * magnetic_sensor_Amount;  // compassMulti 데이터 크기 (예제에서는 96바이트라고 가정)
size_t total_len = len1 + len2;

void loop() {

  ws.cleanupClients();
  
  // WebSocket으로 주기적으로 데이터를 전송
  static unsigned long lastWebSocketSendTime = 0;
  unsigned long currentTime = millis();

  // Serial.println(ESP.getFreeHeap());

  if (currentTime - lastWebSocketSendTime >= acquisitiontimeInterval) {
    // unsigned long a = millis();

    char* data = dhtMulti.getAllSensorData();
    char* data2 = compassMulti.getAllSensorData();

    // unsigned long b = millis();
    
    // Serial.println(b - a);

    char* merged_data = new char[len1 + len2];

    memcpy(merged_data, data, len1);
    memcpy(merged_data + len1, data2, len2);

    // 현재 Unix 시간 가져오기
    unsigned long unixTime = timeClient.getEpochTime(); //수정

    // JSON 형식으로 변환할 데이터를 담을 String 객체 생성
    String payload = "{\"time\":" + String(unixTime) + ",\"result\":[";
    for (int i = 0; i < temp_slave_Amount * temp_sensor_Amount * 4; i += 2) {  
        uint16_t value = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8); // 리틀 엔디언 변환
        payload += String(value);
        
        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < temp_slave_Amount * temp_sensor_Amount * 4) {
            payload += ",";
        }
    }

    payload += ",";

    for (int i = 0; i < magnetic_slave_Amount * magnetic_sensor_Amount * 6; i += 2) {  
        uint16_t value = (uint16_t)data2[i] | ((uint16_t)data2[i + 1] << 8); // 리틀 엔디언 변환
        payload += String(value);
        
        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < magnetic_slave_Amount * magnetic_sensor_Amount * 6) {
            payload += ",";
        }
    }
    payload += "]}";

    if (isMeasuring) {
        if (cur_index >= max_counts){
          cur_index = 1;
          currentFileName = "";
          saveDataToSD(unixTime, merged_data, total_len);

          switched = 1;
        } else {
        cur_index++;
        saveDataToSD(unixTime, merged_data, total_len);
        switched = 1;
        }
    }else if (!isMeasuring && switched == 1){
        cur_index = 0;
        currentFileName = "";
        switched = 0;
    }
    
    delete[] merged_data;

    cur_payload = payload;
    cur_status = sendStatus();
    ws.textAll(payload);

    // 마지막 전송 시간 갱신
    lastWebSocketSendTime = currentTime;

    yield();
  } else if (new_client){
    ws.textAll(cur_status);
    ws.textAll(cur_payload);
    new_client = false;
  }

}
