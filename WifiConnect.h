#ifndef WIFICONNECT_H
#define WIFICONNECT_H

#include <ESP8266WiFi.h>

class WifiConnect {
private:
    const char* ssid = "sorim445_2G";
    const char* password = "earlyuniverse?";

public:
    void connect();
};

#endif
