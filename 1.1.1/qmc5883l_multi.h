#ifndef QMC5883L_MULTI_H
#define QMC5883L_MULTI_H

#include "global.h"
// #include "email.h"

class QMC5883LMulti {
public:
  QMC5883LMulti(uint8_t slaveAmount, uint8_t sensorAmount, int* arr);

  void getAllSensorData();
  char* combinedData;

private:
  uint8_t slaveAmount;
  uint8_t sensorAmount;
  int* arr;
  
  bool requestSensorData(uint8_t slaveId, char* buffer);
  bool validateReceivedData(const char* data, uint8_t slaveId);
};

extern SoftwareSerial hc12;

#endif