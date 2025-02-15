#include "WifiConnect.h"
#include "WebSocketHandler.h"

#include "DHT_multi.h"
#include "qmc5883l_multi.h"

//#include "email.h"

// HC-12 모듈을 위한 SoftwareSerial 객체
SoftwareSerial hc12(D2, D1);

// DHTMulti 객체 생성
DHTMulti dhtMulti(temp_slave_Amount, temp_sensor_Amount, alive_temp_slave);

// QMC5883LMulti 객체 생성
QMC5883LMulti compassMulti(magnetic_slave_Amount, magnetic_sensor_Amount, alive_mag_slave);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

SdFat sd;  // SD 카드 객체

int switched = 0;

size_t len1 = 4 * temp_slave_Amount * temp_sensor_Amount;
size_t len2 = 6 * magnetic_slave_Amount * magnetic_sensor_Amount;
size_t total_len = len1 + len2;
size_t prevHeap = 52400; // 초기 heap값

static char cur_payload[512] = "";
static char cur_status[256] = "";

void setup() {

  Serial.begin(115200);
  hc12.begin(9600);  // HC-12 통신 시작 (메인 함수에서 설정)
  
  // WifiConnect 객체 생성
  WifiConnect wifi;
  wifi.connect();  // WiFi 연결 시도

  // SPIFFS 파일 시스템 초기화
  if (!LittleFS.begin()) {
    // Serial.println("LittleFS Intial Failed");
    return;
  }

  // SD 파일 시스템 초기화
  if (!sd.begin(15, SD_SCK_MHZ(25))) {
    // Serial.println("SD card initialization failed!");
    return;
  }

  setupWebSocket(server, ws);
  server.addHandler(&ws);

  configTime(9 * 3600, 0, "kr.pool.ntp.org", "pool.ntp.org");

  // 서버 시작
  server.begin();
  Serial.println("HTTP server started");

  // sendEmail("NodeMCU Server v1.1.1", "Server Intialized");
  
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
  // "sla": temp_slave_Amount,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sla\":%d,", temp_slave_Amount);
  // "sa": temp_sensor_Amount,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sa\":%d,", temp_sensor_Amount);
  // "ats": [ ... ]
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ats\":[");
  for (int i = 0; i < temp_slave_Amount; i++) {
    offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "%d", alive_temp_slave[i]);
    if (i < temp_slave_Amount - 1) {
      offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, ",");
    }
  }
  // 온도 객체 닫기
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "]},");
  
  // "m": { ... } 자기장 관련 데이터 구성
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"m\":{");
  // "sla": magnetic_slave_Amount,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sla\":%d,", magnetic_slave_Amount);
  // "sa": magnetic_sensor_Amount,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"sa\":%d,", magnetic_sensor_Amount);
  // "ams": [ ... ]
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ams\":[");
  for (int i = 0; i < magnetic_slave_Amount; i++) {
    offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "%d", alive_mag_slave[i]);
    if (i < magnetic_slave_Amount - 1) {
      offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, ",");
    }
  }
  // 자기장 객체 닫기
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "]},");
  
  // "i": isSaving, "c": cur_index,
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"i\":%d,\"c\":%d,", isSaving, cur_index);

  // "d": { ... } 기타 데이터 구성
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"d\":{");
  // "ati": acquisitiontimeInterval, "m": max_counts
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "\"ati\":%d,\"m\":%d", acquisitiontimeInterval, max_counts);
  // d 객체 닫기
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "}");

  // 전체 JSON 객체 닫기
  offset += snprintf(cur_status + offset, sizeof(cur_status) - offset, "}");
  // Serial.println(cur_status);
  ws.textAll(cur_status);
}

void sendPayload(unsigned long ut) {
    // static char payload[512];  // 충분한 크기의 버퍼 설정 (필요 시 조절 가능)
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

    // Serial.println(cur_payload);
    ws.textAll(cur_payload);

    // return payload;  // 생성된 JSON 문자열 반환
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
        // Serial.println("새 파일 생성: " + currentlysavingFile);
    }

    size_t totalSize = 4 + d1_len + d2_len; // Unix Time(4바이트) + d1 + d2 크기
    uint8_t rawData[totalSize];  // 동적 할당 대신 배열 크기를 조정

    memcpy(rawData, &unixTime, 4);         // Unix Time 복사 (4바이트)
    memcpy(rawData + 4, d1, d1_len);       // d1 복사
    memcpy(rawData + 4 + d1_len, d2, d2_len); // d2 복사

    File32 file = sd.open(currentlysavingFile.c_str(), O_RDWR | O_CREAT | O_APPEND);
    if (!file) {
        // Serial.println("파일 열기 실패");
        return;
    }
    
    file.write(rawData, totalSize);  // 전체 데이터 저장
    file.close();
}


void loop() {

  static unsigned long lastWebSocketSendTime = 0;
  unsigned long currentTime = millis();

  if (ESP.getFreeHeap() < prevHeap) {
    Serial.print(ESP.getFreeHeap());
    Serial.print(" ");
    Serial.println(prevHeap - ESP.getFreeHeap());
    prevHeap = ESP.getFreeHeap();
  }

  ws.cleanupClients();

  if ((currentTime - lastWebSocketSendTime >= acquisitiontimeInterval) && !isUpdating) {
    
    lastWebSocketSendTime = currentTime;
    
    // 측정 상태 설정
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
    if (isSaving) {
      if (cur_index >= max_counts) {
        currentlysavingFile = "";
      }
      cur_index++;
      saveDataToSD(lastunixTime, dhtMulti.combinedData, compassMulti.combinedData, len1, len2);
      switched = 1;

    }
    
    // 저장중은 아닐 때, 파일이름 및 저장 데이터 인덱스 초기화
    if (!isSaving && switched == 1) {
      cur_index = 0;
      currentlysavingFile = "";
      switched = 0;
    }

    // 클라이언트 접속 시
    if (client_n > 0) {
      sendStatus();
      sendPayload(lastunixTime);
    }
    
    // 측정상태 해제
    isMeasuring = false;
  }
  
  if (new_client) {
    // ws.textAll(cur_status);
    // ws.textAll(cur_payload);
    new_client = false;
  }
  
  yield();
}
