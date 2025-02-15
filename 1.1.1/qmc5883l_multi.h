#ifndef QMC5883L_MULTI_H
#define QMC5883L_MULTI_H

#include "global.h"
#include "email.h"

// QMC5883LMulti 클래스 정의
class QMC5883LMulti {
public:
  QMC5883LMulti(uint8_t slaveAmount, uint8_t sensorAmount, int* arr);

  // 모든 슬레이브의 센서 데이터를 수집
  void getAllSensorData();
  char* combinedData;  // 수집된 데이터 저장

private:
  uint8_t slaveAmount;
  uint8_t sensorAmount;
  int* arr;
  

  bool requestSensorData(uint8_t slaveId, char* buffer);
  bool validateReceivedData(const char* data, uint8_t slaveId);
};

extern SoftwareSerial hc12;  // 메인에서 선언된 hc12 사용

#endif