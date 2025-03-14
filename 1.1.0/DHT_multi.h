#ifndef DHT_MULTI_H
#define DHT_MULTI_H

#include "global.h"
#include "email.h"

class DHTMulti {
public:
  DHTMulti(uint8_t slaveAmount, uint8_t sensorAmount, int* arr);

  void getAllSensorData();
  char* combinedData;
  
private:
  uint8_t slaveAmount;
  uint8_t sensorAmount;
  int* arr;

  bool requestSensorData(uint8_t slaveId, char* buffer);
  bool validateReceivedData(const char* data, uint8_t slaveId);
};

extern SoftwareSerial hc12;  // 메인에서 선언된 hc12 사용

#endif
