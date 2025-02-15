#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

#include "WifiConnect.h"
#include "WebSocketHandler.h"

#include "DHT_multi.h"
#include "qmc5883l_multi.h"

#include "email.h"

SoftwareSerial hc12(D2, D1);  // HC-12 모듈을 위한 SoftwareSerial 객체

// DHTMulti 객체 생성
DHTMulti dhtMulti(temp_slave_Amount, temp_sensor_Amount, alive_temp_slave);

// QMC5883LMulti 객체 생성
QMC5883LMulti compassMulti(magnetic_slave_Amount, magnetic_sensor_Amount, alive_mag_slave);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

SdFat sd;  // SD 카드 객체

WiFiUDP ntpUDP;                                          // NTP를 위한 UDP 객체 생성
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 3600000);  // UTC 시간 동기화, 1시간 간격 갱신

int switched = 0;

size_t len1 = 4 * temp_slave_Amount * temp_sensor_Amount;          // dhtMulti 데이터 크기 (예제에서는 96바이트라고 가정)
size_t len2 = 6 * magnetic_slave_Amount * magnetic_sensor_Amount;  // compassMulti 데이터 크기 (예제에서는 96바이트라고 가정)
size_t total_len = len1 + len2;
size_t prevHeap = ESP.getFreeHeap();  // 이전 힙 메모리 값 저장

static char cur_payload[512] = "";
static char cur_status[256] = "";

void setup() {

  Serial.begin(115200);
  hc12.begin(9600);  // HC-12 통신 시작 (메인 함수에서 설정)
  delay(1000);
  
  // WifiConnect 객체 생성
  WifiConnect wifi;
  wifi.connect();  // WiFi 연결 시도

  // SPIFFS 파일 시스템 초기화
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Intial Failed");
    return;
  }

  // SD 파일 시스템 초기화
  if (!sd.begin(15, SD_SCK_MHZ(25))) {
    Serial.println("SD card initialization failed!");
    return;
  }

  setupWebSocket(server, ws);
  server.addHandler(&ws);

  // NTPClient 시작
  timeClient.begin();
  while (!timeClient.update()) {
  }

  // 서버 시작
  server.begin();
  Serial.println("HTTP server started");

  sendEmail("NodeMCU Server v1.1.0", "Server Intialized");
  
  Serial.println(ESP.getFreeHeap());
}


char* sendStatus() {
  StaticJsonDocument<256> doc;

  doc["v"] = version;

  JsonObject temp = doc.createNestedObject("t");
  temp["sla"] = temp_slave_Amount;
  temp["sa"] = temp_sensor_Amount;

  JsonArray temp_slaves = temp.createNestedArray("ats");
  for (int i = 0; i < temp_slave_Amount; i++) {
    temp_slaves.add(alive_temp_slave[i]);
  }

  JsonObject magnetic = doc.createNestedObject("m");
  magnetic["sla"] = magnetic_slave_Amount;
  magnetic["sa"] = magnetic_sensor_Amount;

  JsonArray mag_slaves = magnetic.createNestedArray("ams");
  for (int i = 0; i < magnetic_slave_Amount; i++) {
    mag_slaves.add(alive_mag_slave[i]);
  }

  doc["i"] = isSaving;
  doc["c"] = cur_index;

  JsonObject data = doc.createNestedObject("d");
  data["ati"] = acquisitiontimeInterval;
  data["m"] = max_counts;

  static char status[256];
  serializeJson(doc, status, sizeof(status));
  ws.textAll(status);

  return status;
}

char* sendPayload(unsigned long ut) {
    static char payload[512];  // 충분한 크기의 버퍼 설정 (필요 시 조절 가능)
    size_t offset = 0;  // 현재 payload에서 문자열이 추가될 위치

    // JSON 시작
    offset += snprintf(payload + offset, sizeof(payload) - offset, "{\"time\":%lu,\"result\":[", ut);

    // 온도 센서 데이터 추가
    for (int i = 0; i < temp_slave_Amount * temp_sensor_Amount * 4; i += 2) {
        uint16_t value = (uint16_t)dhtMulti.combinedData[i] | ((uint16_t)dhtMulti.combinedData[i + 1] << 8);
        offset += snprintf(payload + offset, sizeof(payload) - offset, "%d", value);

        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < temp_slave_Amount * temp_sensor_Amount * 4 || magnetic_slave_Amount * magnetic_sensor_Amount * 6 > 0) {
            offset += snprintf(payload + offset, sizeof(payload) - offset, ",");
        }
    }

    // 자기 센서 데이터 추가
    for (int i = 0; i < magnetic_slave_Amount * magnetic_sensor_Amount * 6; i += 2) {
        uint16_t value = (uint16_t)compassMulti.combinedData[i] | ((uint16_t)compassMulti.combinedData[i + 1] << 8);
        offset += snprintf(payload + offset, sizeof(payload) - offset, "%d", value);

        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < magnetic_slave_Amount * magnetic_sensor_Amount * 6) {
            offset += snprintf(payload + offset, sizeof(payload) - offset, ",");
        }
    }

    // JSON 닫기
    snprintf(payload + offset, sizeof(payload) - offset, "]}");

    // WebSocket을 통해 전송
    ws.textAll(payload);

    return payload;  // 생성된 JSON 문자열 반환
}


void saveDataToSD(unsigned long unixTime, const char* d1, const char* d2, size_t d1_len, size_t d2_len) {
    if (currentlysavingFile == "") {
        time_t rawTime = unixTime + GMT;
        struct tm* timeInfo = localtime(&rawTime);

        char formattedTime[30];
        strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", timeInfo);

        snprintf(formattedTime, sizeof(formattedTime), "%s_%d_%d_%d_%d.dat",
                 formattedTime, temp_slave_Amount, temp_sensor_Amount, magnetic_slave_Amount, magnetic_sensor_Amount);
        currentlysavingFile = formattedTime;
        Serial.println("새 파일 생성: " + currentlysavingFile);
    }

    size_t totalSize = 4 + d1_len + d2_len; // Unix Time(4바이트) + d1 + d2 크기
    uint8_t rawData[totalSize];  // 동적 할당 대신 배열 크기를 조정

    memcpy(rawData, &unixTime, 4);         // Unix Time 복사 (4바이트)
    memcpy(rawData + 4, d1, d1_len);       // d1 복사
    memcpy(rawData + 4 + d1_len, d2, d2_len); // d2 복사

    File32 file = sd.open(currentlysavingFile.c_str(), O_RDWR | O_CREAT | O_APPEND);
    if (!file) {
        Serial.println("파일 열기 실패");
        return;
    }
    
    file.write(rawData, totalSize);  // 전체 데이터 저장
    file.close();
}


void loop() {

  static unsigned long lastWebSocketSendTime = 0;
  unsigned long currentTime = millis();

  if (ESP.getFreeHeap() < prevHeap) {
    Serial.println(ESP.getFreeHeap());
    prevHeap = ESP.getFreeHeap();
  }

  if (ws.count() > 0) {
    ws.cleanupClients();
  }

  if ((currentTime - lastWebSocketSendTime >= acquisitiontimeInterval) && !isUpdating) {
    
    lastWebSocketSendTime = currentTime;
    isMeasuring = true;

    unsigned long unixTime = timeClient.getEpochTime();
    static unsigned long lastunixTime;

    if (unixTime == 0) {
      lastunixTime += acquisitiontimeInterval / 1000;
    } else {
      lastunixTime = unixTime;
    }

    dhtMulti.getAllSensorData();
    compassMulti.getAllSensorData();

    // 클라이언트 접속 시
    if (client_n > 0) {

      strncpy(cur_status, sendStatus(), sizeof(cur_status) - 1);
      cur_status[sizeof(cur_status) - 1] = '\0';
      
      strncpy(cur_payload, sendPayload(lastunixTime), sizeof(cur_payload) - 1);
      cur_payload[sizeof(cur_payload) - 1] = '\0';
    }
    
    // 저장 중일 시
    if (isSaving) {
      if (cur_index >= max_counts) {
        currentlysavingFile = "";
      }
      cur_index++;
      saveDataToSD(lastunixTime, dhtMulti.combinedData, compassMulti.combinedData, len1, len2);
      switched = 1;

    } else if (!isSaving && switched == 1) {
      cur_index = 0;
      currentlysavingFile = "";
      switched = 0;
    }

    isMeasuring = false;

  }
  
  if (new_client) {
    ws.textAll(cur_status);
    ws.textAll(cur_payload);
    new_client = false;
  }

}
