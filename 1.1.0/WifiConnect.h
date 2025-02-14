#ifndef WIFICONNECT_H
#define WIFICONNECT_H

#include "global.h"

class WifiConnect {
private:
  const char* ssid = "U+Net67B0";
  const char* password = "physical3!4";

public:
  void connect();
};

#endif
