#include "serial_cmd.h"

void serialcmd() {

  if (Serial.available()){
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "r") {
      if (!isSDoff) {
        SPI.end();
        digitalWrite(15, HIGH);
        DEBUG_PRINTLN("SD end & Device reset");
      }
      ESP.restart();

    } else if (cmd == "s") {
      if (!isSDoff) {
          isSDoff = true;
          SPI.end();
          digitalWrite(15, HIGH);
          DEBUG_PRINTLN("SD end");
      }
    } else if (cmd == "stat"){
      DEBUG_PRINTLN(isSDoff);
    }
  }
}

