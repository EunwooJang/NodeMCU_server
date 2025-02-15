#include "WifiConnect.h"

void WifiConnect::connect() {
  WiFi.begin("sorim445_2G", "earlyuniverse?");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}
