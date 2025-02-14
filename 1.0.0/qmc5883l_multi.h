#ifndef QMC5883L_MULTI_H
#define QMC5883L_MULTI_H

#include <Arduino.h>
#include <SoftwareSerial.h>

// QMC5883LMulti 클래스 정의
class QMC5883LMulti {
public:
  QMC5883LMulti(uint8_t slaveAmount, uint8_t sensorAmount, int* arr);

  // 모든 슬레이브의 센서 데이터를 수집
  char* getAllSensorData();

private:
  uint8_t slaveAmount;    // 슬레이브 개수
  uint8_t sensorAmount;
  int* arr;
  char* combinedData;     // 수집된 데이터 저장

  bool requestSensorData(uint8_t slaveId, char* buffer);
  bool validateReceivedData(const char* data, uint8_t slaveId);
};

extern SoftwareSerial hc12; // 메인에서 선언된 hc12 사용

#endif
