#include "WifiConnect.h"
#include "WebSocketHandler.h"

#include "DHT_multi.h"
#include "qmc5883l_multi.h"

#include "serial_cmd.h"

// HC-12 모듈을 위한 SoftwareSerial 객체
SoftwareSerial hc12(D2, D1);

// DHTMulti 객체 생성
DHTMulti dhtMulti(temp_slave_Amount, temp_sensor_Amount);

// QMC5883LMulti 객체 생성
QMC5883LMulti compassMulti(magnetic_slave_Amount, magnetic_sensor_Amount);

// SD 카드 객체
SdFat sd;

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

size_t prevHeap = 52400; // 초기 heap값

bool switched = false; // SD 파일 전환 여부

size_t len1 = 4 * temp_slave_Amount * temp_sensor_Amount;
size_t len2 = 6 * magnetic_slave_Amount * magnetic_sensor_Amount;
size_t total_len = len1 + len2;

static char cur_payload[512] = "";
static char cur_status[256] = "";

void setup() {

  pinMode(15, OUTPUT); // SD end

  DEBUG_BEGIN(74880); // 디버깅용 시리얼 통신 시작
  hc12.begin(9600);  // HC-12 통신 시작 (메인 함수에서 설정)
  
  // SPIFFS 파일 시스템 초기화
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS Intial Failed");
    return;
  }

  // SD 파일 시스템 초기화
  if (!sd.begin(15, SD_SCK_MHZ(25))) {
    DEBUG_PRINTLN("SD card initialization failed!");
    SPI.end();  // SPI 버스 비활성화
    digitalWrite(15, HIGH);  // CS 핀을 HIGH로 설정하여 SD 카드 비활성화
    delay(5000);
    ESP.restart();
    return;
  }

  WifiConnect wifi;
  wifi.connect();

  setupWebSocket(server, ws);

  configTime(9 * 3600, 0, "kr.pool.ntp.org", "pool.ntp.org");

  // 서버 시작
  server.begin();
  DEBUG_PRINTLN("HTTP server started");

  sendEmail("NodeMCU Server v1.1.1", "Server Intialized");
  
  prevHeap = ESP.getFreeHeap();
}

void sendStatus() {
  // JSON 문자열을 저장할 오프셋 변수 (현재까지 작성된 문자열 길이)
  int offset = 0;

  // JSON 객체의 시작
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "{");

  // "v": version,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"v\":\"%s\",", version);

  // "t": { ... } 온도 관련 데이터 구성
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"t\":{");
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sla\":%d,", temp_slave_Amount);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sa\":%d,", temp_sensor_Amount);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ats\":[");
  for (int i = 0; i < temp_slave_Amount; i++) {
    offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "%d", alive_temp_slave[i]);
    if (i < temp_slave_Amount - 1) {
      offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, ",");
    }
  }
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "]},");

  // "m": { ... } 자기장 관련 데이터 구성
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"m\":{");
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sla\":%d,", magnetic_slave_Amount);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sa\":%d,", magnetic_sensor_Amount);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ams\":[");
  for (int i = 0; i < magnetic_slave_Amount; i++) {
    offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "%d", alive_mag_slave[i]);
    if (i < magnetic_slave_Amount - 1) {
      offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, ",");
    }
  }
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "]},");

  // "i": isSaving, "c": cur_index,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"i\":%d,\"c\":%d,", isSaving, cur_index);

  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"d\":{");
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ati\":%d,\"m\":%d,", acquisitiontimeInterval, max_counts);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"heap\":%zu,", prevHeap);
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sdoff\":%d", isSDoff);

  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "}");

  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "}");

  DEBUG_PRINTLN("JSON 크기: " + String(offset));

  ws.textAll(cur_status);
}

void sendPayload(unsigned long ut) {
    size_t offset = 0;  // 현재 payload에서 문자열이 추가될 위치

    // JSON 시작
    offset += snprintf(cur_payload + offset, sizeof(cur_payload) - offset, "{\"time\":%lu,\"result\":[", ut);

    // 온도 센서 데이터 추가
    for (int i = 0; i < temp_slave_Amount * temp_sensor_Amount * 4; i += 2) {
        uint16_t value = (uint16_t)dhtMulti.combinedData[i] | ((uint16_t)dhtMulti.combinedData[i + 1] << 8);
        offset += snprintf(cur_payload + offset, sizeof(cur_payload) - offset, "%d", value);

        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < temp_slave_Amount * temp_sensor_Amount * 4 || magnetic_slave_Amount * magnetic_sensor_Amount * 6 > 0) {
            offset += snprintf(cur_payload + offset, sizeof(cur_payload) - offset, ",");
        }
    }

    // 자기 센서 데이터 추가
    for (int i = 0; i < magnetic_slave_Amount * magnetic_sensor_Amount * 6; i += 2) {
        uint16_t value = (uint16_t)compassMulti.combinedData[i] | ((uint16_t)compassMulti.combinedData[i + 1] << 8);
        offset += snprintf(cur_payload + offset, sizeof(cur_payload) - offset, "%d", value);

        // 마지막 요소가 아니라면 쉼표 추가
        if (i + 2 < magnetic_slave_Amount * magnetic_sensor_Amount * 6) {
            offset += snprintf(cur_payload + offset, sizeof(cur_payload) - offset, ",");
        }
    }

    snprintf(cur_payload + offset, sizeof(cur_payload) - offset, "]}");

    ws.textAll(cur_payload);

}

void saveDataToSD(unsigned long unixTime, const char* d1, const char* d2, size_t d1_len, size_t d2_len) {
    if (currentlysavingFile == "") {
        time_t rawTime = unixTime;
        struct tm* timeInfo = localtime(&rawTime);

        char formattedTime[30];
        strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", timeInfo);

        snprintf(formattedTime, sizeof(formattedTime), "%s_%d_%d_%d_%d.dat",
                 formattedTime, temp_slave_Amount, temp_sensor_Amount, magnetic_slave_Amount, magnetic_sensor_Amount);
        currentlysavingFile = formattedTime;
        DEBUG_PRINTLN("새 파일 생성: " + currentlysavingFile);
    }

    size_t totalSize = 4 + d1_len + d2_len;
    uint8_t rawData[totalSize];

    memcpy(rawData, &unixTime, 4);
    memcpy(rawData + 4, d1, d1_len);
    memcpy(rawData + 4 + d1_len, d2, d2_len);

    File32 file = sd.open(currentlysavingFile.c_str(), O_WRONLY | O_CREAT | O_APPEND);
    if (!file) {
        SPI.end();  // SPI 버스 비활성화
        digitalWrite(15, HIGH);
        DEBUG_PRINTLN("파일 열기 실패");
        delay(5000);
        ESP.restart();
        return;
    }
    
    file.write(rawData, totalSize);  // 전체 데이터 저장
    file.close();
}


void loop() {

  // Serial command
  // serialcmd();
  
  static unsigned long lastWebSocketSendTime = 0;
  unsigned long currentTime = millis();

  if (ESP.getFreeHeap() < prevHeap) {
    DEBUG_PRINT(ESP.getFreeHeap());
    DEBUG_PRINT(" ");
    DEBUG_PRINTLN(prevHeap - ESP.getFreeHeap());
    prevHeap = ESP.getFreeHeap();
  }

  ws.cleanupClients();

  if ((currentTime - lastWebSocketSendTime >= acquisitiontimeInterval) && !isUpdating) {
      
    lastWebSocketSendTime = currentTime;
    
    // 측정 상태 설정
    DEBUG_PRINTLN("Measuring...");
    isMeasuring = true;

    struct tm timeinfo;
    unsigned long unixTime;
    static unsigned long lastunixTime;

    // 서버 시간 얻기
    if (getLocalTime(&timeinfo)) {
      unixTime = mktime(&timeinfo);
     } else {
      unixTime = 0;
    }

    // 서버 시간 얻기 성공/실패 유무에 따른 처리
    if (unixTime == 0) {
      lastunixTime += acquisitiontimeInterval / 1000;
     } else {
      lastunixTime = unixTime;
    }

    // 센서 데이터 얻기
    dhtMulti.getAllSensorData();
    compassMulti.getAllSensorData();
    
    // 저장 중일 시
    if (isSaving && !isSDoff) {
      if (cur_index >= max_counts) {
        currentlysavingFile = "";
      }
      cur_index++;
      DEBUG_PRINTLN(cur_index);
      saveDataToSD(lastunixTime, dhtMulti.combinedData, compassMulti.combinedData, len1, len2);
      switched = true;
    }
    
    // 저장중은 아닐 때, 파일이름 및 저장 데이터 인덱스 초기화
    if (!isSaving && switched) {
      cur_index = 0;
      currentlysavingFile = "";
      switched = false;
    }

    // 클라이언트 접속 시
    if (client_n > 0) {
      sendStatus();
      sendPayload(lastunixTime);
    }

    // 측정상태 해제
    isMeasuring = false;
    
    DEBUG_PRINTLN("Non Measuring...");

  }

  yield();
}
