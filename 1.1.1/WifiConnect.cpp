#include "WifiConnect.h"

void Wificonnect() {
  // 저수준 파일
  File32 file = sd.open("wifi.txt", O_READ);
  if (!file) {
      DEBUG_PRINTLN("Failed to open wifi.txt");
      return;
  }

  char ssid[20], password[20];
  int line = 0;
  bool connected = false;

  while (file.available()) {
    String data = file.readStringUntil('\n');  // 한 줄씩 읽음
    data.trim();  // 개행 문자 제거

    if (line % 2 == 0) strncpy(ssid, data.c_str(), sizeof(ssid) - 1); // SSID 저장
    else {
      strncpy(password, data.c_str(), sizeof(password) - 1);  // PASSWORD 저장
        
      // WiFi 연결 시도
      DEBUG_PRINT("\nConnecting to: ");
      DEBUG_PRINTLN(ssid);
      DEBUG_PRINTLN(password);

      WiFi.begin(ssid, password);
        
      unsigned long startTimemillis = millis();
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DEBUG_PRINT(".");
            
        // 30초(30000ms) 동안 연결되지 않으면 다음 SSID로 이동
        if (millis() - startTimemillis > 30000) {
          DEBUG_PRINTLN("\nConnection Timeout! Trying next...");
          break;
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("\nConnected!");
        DEBUG_PRINT("IP Address: ");
        DEBUG_PRINTLN(WiFi.localIP());
        connected = true;
        break;  // 연결 성공 시 루프 종료
      }
    }
    
    line++;
  }
  
  file.close();

  if (!connected) {
    DEBUG_PRINTLN("No available WiFi networks connected.");
  }
  
  // 만일 SD카드에 들어있는 와이파이 정보들로 접속 불가 시, 아래의 코드를 실행해야 함
  /*
  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("IP Address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  */
}
