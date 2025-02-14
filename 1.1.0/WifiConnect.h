#ifndef WIFICONNECT_H
#define WIFICONNECT_H

#include "global.h"

class WifiConnect {
private:
  const char* ssid = "SSID";
  const char* password = "PASSWORD";

public:
  void connect();
};

#endif
