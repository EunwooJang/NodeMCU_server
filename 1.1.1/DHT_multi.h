#ifndef DHT_MULTI_H
#define DHT_MULTI_H

#include "global.h"
#include "email.h"

class DHTMulti {
public:
  DHTMulti(uint8_t slaveAmount, uint8_t sensorAmount);

  void getAllSensorData();
  char* combinedData;
  
private:
  uint8_t slaveAmount;
  uint8_t sensorAmount;

  bool requestSensorData(uint8_t slaveId, char* buffer);
  bool validateReceivedData(const char* data, uint8_t slaveId);
};

extern SoftwareSerial hc12;

#endif
