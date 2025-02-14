#include "qmc5883l_multi.h"

// 외부에서 선언된 HC12 참조
extern SoftwareSerial hc12;

QMC5883LMulti::QMC5883LMulti(uint8_t slaveAmount, uint8_t sensorAmount, int* arr)
  : slaveAmount(slaveAmount), sensorAmount(sensorAmount) {
  // 전달받은 배열을 복사하여 저장
  this->arr = new int[slaveAmount];  // 필요에 따라 크기 조정
  for (int i = 0; i < slaveAmount; i++) {
    this->arr[i] = arr[i];  // 배열 복사
  }
  combinedData = new char[6 * sensorAmount * slaveAmount];
  memset(combinedData, 0, 6 * sensorAmount * slaveAmount);
}

void QMC5883LMulti::getAllSensorData() {
  memset(combinedData, 0, 6 * sensorAmount * slaveAmount);

  // 전체 슬레이브에게 측정하라고 명령 전달
  char command[5];
  snprintf(command, sizeof(command), "SAMD");
  hc12.write(command);  // 문자열의 실제 길이만큼 전송
  hc12.flush();
  delay(300);  // 슬레이브가 데이터 수집할 동안 대기

  for (uint8_t i = 1; i <= slaveAmount; i++) {

    int count = 0;  // 각 슬레이브 마다 요청할 최대 횟수는 5회
    char buffer[4 + 6 * sensorAmount];
    memset(buffer, 0, sizeof(buffer));
    if (arr[i - 1] == 1) {
      while (count < 4) {
        if (requestSensorData(i, buffer)) {
          memcpy(&combinedData[(i - 1) * 6 * sensorAmount], buffer + 4, 6 * sensorAmount);
          break;
        }
        count++;
      }
    }
    if (count == 4) {
      char message[32];
      snprintf(message, sizeof(message), "Temperature Slave ID %d fail", i);
      sendEmail("TEST NodeMCU Server v1.1.0", message);
    }
  }
}

bool QMC5883LMulti::requestSensorData(uint8_t slaveId, char* buffer) {
  char command[5];
  snprintf(command, 5, "S%dMD", slaveId);
  hc12.write(command);
  hc12.flush();

  unsigned long startTime = millis();
  while ((millis() - startTime) < 500) {  // 200ms 대기
    if (hc12.available() >= 4 + 6 * sensorAmount) {
      hc12.readBytes(buffer, 4 + 6 * sensorAmount);
      return validateReceivedData(buffer, slaveId);
    }
  }

  while (hc12.available()) {
    hc12.read();
  }

  return false;  // 타임아웃
}

bool QMC5883LMulti::validateReceivedData(const char* data, uint8_t slaveId) {
  char expectedHeader[5];
  snprintf(expectedHeader, sizeof(expectedHeader), "D%dMD", slaveId);
  return strncmp(data, expectedHeader, 4) == 0;
}
