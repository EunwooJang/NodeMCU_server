#include "WifiConnect.h"

void WifiConnect::connect() {
  // WiFi.begin("sorim445_2G", "earlyuniverse?");
  WiFi.begin("U+Net67B0", "physical3!4");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("IP Address: ");
  DEBUG_PRINTLN(WiFi.localIP());
}
