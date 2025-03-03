#include "DHT_multi.h"

DHTMulti::DHTMulti(uint8_t slaveAmount, uint8_t sensorAmount)
  : slaveAmount(slaveAmount), sensorAmount(sensorAmount) {
  combinedData = new char[4 * sensorAmount * slaveAmount];
  memset(combinedData, 0, 4 * sensorAmount * slaveAmount);
}

void DHTMulti::getAllSensorData() {
  // 데이터 저장 값 초기화
  memset(combinedData, 0, 4 * sensorAmount * slaveAmount);

  // 전체 슬레이브에게 측정하라고 명령 전달
  char command[5];
  snprintf(command, sizeof(command), "SATD");
  hc12.write(command);  // 문자열의 실제 길이만큼 전송
  hc12.flush();
  delay(100);  // 슬레이브가 데이터 수집할 동안 대기

  for (uint8_t i = 1; i <= slaveAmount; i++) {

    int count = 0;  // 각 슬레이브 마다 요청할 최대 횟수는 4회
    char buffer[4 + 4 * sensorAmount];
    memset(buffer, 0, sizeof(buffer));
    if (alive_temp_slave[i - 1]) {
      while (count < 4) {
        if (requestSensorData(i, buffer)) {
          memcpy(&combinedData[(i - 1) * 4 * sensorAmount], buffer + 4, 4 * sensorAmount);
          break;
        }
        count++;
      }
    }
    if (count == 4) {
      DEBUG_PRINT("TEMP SLAVE ID: ");
      DEBUG_PRINT(i);
      DEBUG_PRINT(" ");
      DEBUG_PRINTLN("Receiving Fail");
      char message[32];
      snprintf(message, sizeof(message), "Temperature Slave ID %d fail", i);
      sendEmail("TEST NodeMCU Server v1.1.0", message);
    }
  }
}

bool DHTMulti::requestSensorData(uint8_t slaveId, char* buffer) {
  char command[5];
  snprintf(command, 5, "S%dTD", slaveId);
  hc12.write(command);
  hc12.flush();

  unsigned long startTimemillis = millis();
  while ((millis() - startTimemillis) < 500) {  // 500ms 대기
    if (hc12.available() >= 4 + 4 * sensorAmount) {
      hc12.readBytes(buffer, 4 + 4 * sensorAmount);
      return validateReceivedData(buffer, slaveId);
    }
  }

  while (hc12.available()) {
    hc12.read();
  }

  return false;  // 타임아웃
}

bool DHTMulti::validateReceivedData(const char* data, uint8_t slaveId) {
  char expectedHeader[5];
  snprintf(expectedHeader, sizeof(expectedHeader), "D%dTD", slaveId);
  return strncmp(data, expectedHeader, 4) == 0;
}
