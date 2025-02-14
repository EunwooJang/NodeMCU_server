#ifndef WIFICONNECT_H
#define WIFICONNECT_H

#include <ESP8266WiFi.h>

class WifiConnect {
private:
    const char* ssid = "SSID";
    const char* password = "PASSWORD";

public:
    void connect();
};

#endif
