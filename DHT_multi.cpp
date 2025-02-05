#include "DHT_multi.h"
#include <Arduino.h>
#include <SoftwareSerial.h>

extern SoftwareSerial hc12; // 메인에서 선언된 hc12 사용

DHTMulti::DHTMulti(uint8_t slaveAmount)
  : slaveAmount(slaveAmount) {
  combinedData = new char[20 * slaveAmount];
  memset(combinedData, 0, 20 * slaveAmount);
}

char* DHTMulti::getAllSensorData() {

  memset(combinedData, 0, 20 * slaveAmount);

  // 전체 슬레이브에게 측정하라고 명령 전달
  char command[5]; 
  snprintf(command, sizeof(command), "SATD"); 
  hc12.write(command); // 문자열의 실제 길이만큼 전송
  hc12.flush();
  delay(1000); // 슬레이브가 데이터 수집할 동안 대기


  for (uint8_t i = 1; i <= slaveAmount; i++) {
    
    int count = 0; // 각 슬레이브 마다 요청할 최대 횟수는 3회
    char buffer[24];
    memset(buffer, 0, sizeof(buffer));
    
    while (count < 5) {
        if (requestSensorData(i, buffer)){
          memcpy(&combinedData[(i - 1) * 20], buffer + 4, 20);
          // Serial.print("Y");
          //while 문 탈출
          break;
        }
        count++;
        // Serial.print("N");
    }

    // Serial.print(" ");
    // delay(1000);
  }
  return combinedData;
}

// 데이터 요청
bool DHTMulti::requestSensorData(uint8_t slaveId, char* buffer) {
  char command[5];
  snprintf(command, 5, "S%dTD", slaveId);
  hc12.write(command);
  hc12.flush();

  unsigned long startTime = millis();
  while ((millis() - startTime) < 500) { // 200ms 대기
    if (hc12.available() >= 24) {
      hc12.readBytes(buffer, 24);
      return validateReceivedData(buffer, slaveId);
    }
  }
    
  while (hc12.available()) {
    hc12.read();
  }

  return false; // 타임아웃
}


// 데이터 유효성 검사
bool DHTMulti::validateReceivedData(const char* data, uint8_t slaveId) {
  char expectedHeader[5];
  snprintf(expectedHeader, sizeof(expectedHeader), "D%dTD", slaveId);
  return strncmp(data, expectedHeader, 4) == 0;
}