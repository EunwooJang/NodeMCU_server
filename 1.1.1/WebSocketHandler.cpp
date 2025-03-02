#include "WebSocketHandler.h"


const char *version = "1.1.1";

int temp_slave_Amount = 4;   
int temp_sensor_Amount = 5;

int magnetic_slave_Amount = 2; // 4;
int magnetic_sensor_Amount = 1; // 3;

int cur_index = 0;

unsigned long acquisitiontimeIntervalmillis = 10000;

int client_n = 0;

int max_counts = 259200;

String currentlysavingFile = "";

// OTA 업데이트용 파일 크기
static size_t update_content_len = 0;

// 과부화 방지를 위해 페이지 접속 간 로드 시간 할당
unsigned long lastRequestTimemillis = 0;
const unsigned long requestIntervalmillis = 500;


void setupWebSocket(AsyncWebServer &server, AsyncWebSocket &ws) {
  // HTTP 메인 페이지
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTimemillis < requestIntervalmillis || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTimemillis = millis();
    request->send(LittleFS, "/index.html", "text/html");
  });

  // HTTPS 메인 페이지
  server.on("/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTimemillis < requestIntervalmillis || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTimemillis = millis();
    request->send(LittleFS, "/indexraw.html", "text/html");
  });

  // favicon 등록 : 존재하면 제공, 없으면 204(No Content) 응답
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  // 웹소켓 핸들러 등록
  server.addHandler(&ws);

  // WebSocket 이벤트 핸들러 등록 ("/ws" 경로는 ws 객체가 생성될 때 지정됨)
  ws.onEvent(
    [](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client_n++;
      
      DEBUG_PRINT(F("WebSocket Client Connected"));
      DEBUG_PRINT(" ");
      DEBUG_PRINTLN(client_n);

    } else if (type == WS_EVT_DISCONNECT) {
      client_n--;

      DEBUG_PRINT(F("WebSocket Client Disconnected"));
      DEBUG_PRINT(" ");
      DEBUG_PRINTLN(client_n);

    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->opcode == WS_TEXT) {
        char message[len + 1];
        memcpy(message, data, len);
        message[len] = '\0';  // 문자열 종료

        // 저장 시작 로직 실행
        if (strcmp(message, "start") == 0) {
          isSaving = true;
          DEBUG_PRINTLN(F("Starting saving process..."));

        // 저장 중지 로직 실행  
        } else if (strcmp(message, "stop") == 0) {
          isSaving = false;
          DEBUG_PRINTLN(F("Stopping saving process..."));
          
        // 기기 초기화
        } else if (strcmp(message, "reset") == 0) {
          if (!isSDoff) {
            SPI.end();  // SPI 버스 비활성화
            digitalWrite(15, HIGH);
          }
          ESP.restart();

        // 전원 해제 이전에 SD카드 끄기
        } else if ((strcmp(message, "sdoff") == 0) && (!isSDoff) && (!isSaving)) {
          isSDoff = true;
          SPI.end();  // SPI 버스 비활성화
          digitalWrite(15, HIGH);  // CS 핀을 HIGH로 설정하여 SD 카드 비활성화
          DEBUG_PRINTLN("SD end");

        // SD카드 키기
        } else if ((strcmp(message, "sdon") == 0) && (isSDoff) && (!isSaving)) {
          isSDoff = false;
          sd.begin(15, SD_SCK_MHZ(25));
          DEBUG_PRINTLN("SD begin");

        // 특정 센서 슬레이브 활성화/비활성화
        } else if (strncmp(message, "turn ", 5) == 0) {
          char type = message[5];  // 't' 또는 'm'
          int index = message[7] - '0';  // 인덱스 숫자
          int value = message[9] - '0';  // 값 (0 또는 1)
          if (type == 't' && index >= 1 && index <= temp_slave_Amount) {
              alive_temp_slave[index - 1] = (value == 1);
          } else if (type == 'm' && index >= 1 && index <= magnetic_slave_Amount) {
              alive_mag_slave[index - 1] = (value == 1);
          }

        // 파일당 저장할 데이터 크기 지정
        } else if (strncmp(message, "set ", 4) == 0 && !isSaving) {
          String numberPart = String(message + 4); // "set " 이후의 문자열 추출
          numberPart.trim(); // 앞뒤 공백 제거
          
          if (numberPart.length() > 0) {
            int value = numberPart.toInt(); // 문자열을 정수로 변환
            
            if (value > 0) { // 유효한 숫자인지 확인
              max_counts = value; // 저장
            }
          }

        // 샘플링 간격 조절 (최소 10초)
        } else if  (strncmp(message, "adjust ", 7) == 0 && !isSaving) {
          while (!isMeasuring) {
          }
          String numberPart = String(message + 7); // "set " 이후의 문자열 추출
          numberPart.trim(); // 앞뒤 공백 제거

          if (numberPart.length() > 0) {
            int newacquisitiontimeIntervalsec = numberPart.toInt(); // 문자열을 정수로 변환
            
            if (newacquisitiontimeIntervalsec >= 10) { // 유효한 숫자인지 확인
              acquisitiontimeIntervalmillis = newacquisitiontimeIntervalsec * 1000; // 저장
            }
          }
        }
      }
    }
  });

  // GET 요청: 클라이언트가 /update 경로로 접속하면, update.html 페이지를 제공
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTimemillis < requestIntervalmillis || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTimemillis = millis();
    request->send(LittleFS, "/update.html", "text/html");
  });

  // POST 요청: 클라이언트가 /update 경로로 펌웨어 파일을 업로드하면 OTA 업데이트 처리
  server.on(
    "/update", HTTP_POST,
    // 첫 번째 콜백: 요청이 끝난 후 호출되지만 여기서는 별도의 응답 처리를 하지 않음
    [](AsyncWebServerRequest *request) {},
    // 두 번째 콜백: 파일 업로드 데이터를 청크 단위로 처리하는 핸들러
    [](AsyncWebServerRequest *request,
       const String &filename,
       size_t index,
       uint8_t *data,
       size_t len,
       bool final) {
      isUpdating = true;

      // 첫 번째 청크(파일의 시작)인 경우 초기화 작업 수행
      if (index == 0) {
        DEBUG_PRINTLN(F("Update started"));
        update_content_len = request->contentLength();

        // 비동기 OTA 업데이트 모드 활성화
        Update.runAsync(true);
        // U_FLASH 영역에 업데이트를 시작, 실패 시 에러 출력 후 종료
        if (!Update.begin(update_content_len, U_FLASH)) {
          Update.printError(Serial);
          return;
        }
      }

      // 현재 청크의 데이터를 플래시 메모리에 기록, 기록 실패 시 에러 출력
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }

      // 마지막 청크인 경우 업데이트 마무리 처리
      if (final) {
        // 업데이트 마무리 및 검증, 실패 시 에러 출력
        if (!Update.end(true)) {
          Update.printError(Serial);
        } else {
          DEBUG_PRINTLN(F("Update complete"));
          Serial.flush();
          ESP.restart();
        }
      }
  });
  
  // 파일 메니저 페이지
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTimemillis < requestIntervalmillis || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTimemillis = millis();
    request->send(LittleFS, "/setup.html", "text/html");
  });

  // LittleFS LIST
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    while (millis() - lastRequestTimemillis < requestIntervalmillis) {
    }
    lastRequestTimemillis = millis();
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"files\":[");
    Dir dir = LittleFS.openDir("/");
    bool first = true;
    while (dir.next()) {
      if (!first) response->print(",");
      response->printf("{\"name\":\"%s\",\"size\":%u}", dir.fileName().c_str(), dir.fileSize());
      first = false;
    }
    response->print("]}");
    request->send(response);

  });

  // LittleFS INFO
  server.on("/spiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
    while (millis() - lastRequestTimemillis < requestIntervalmillis) {
    }
    lastRequestTimemillis = millis();
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    FSInfo fs_info;
    LittleFS.info(fs_info);
    response->printf("{\"total\":%u}", fs_info.totalBytes);
    request->send(response);
  });

  // LittleFS UPLOAD
  server.on(
    "/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "File uploaded");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        File file = LittleFS.open("/" + filename, "w");
        if (!file) {
          DEBUG_PRINTLN(F("Failed to open file for writing"));
          return;
        }
        file.close();
      }
      File file = LittleFS.open("/" + filename, "a");
      if (file) {
        file.write(data, len);
        file.close();
      }
  });

  // LittleFS DELETE
  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("filename", true)) {
      String filename = request->getParam("filename", true)->value();
      if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
        request->send(200, "text/plain", "File deleted");
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  // LittleFS DOWNLOAD
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("filename")) {
      String filename = request->getParam("filename")->value();
      if (LittleFS.exists(filename)) {
        request->send(LittleFS, filename, "application/octet-stream");
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  // SD INFO
  server.on("/sdinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    while ((millis() - lastRequestTimemillis < requestIntervalmillis) && !isMeasuring) {
    }
    lastRequestTimemillis = millis();

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (!sd.card()) {
      isSDoff = true;
      response->print("{\"total\":0}");
    } else {
      uint64_t cardSize = sd.card()->sectorCount();
      uint64_t cardCapacity = cardSize * 512;
      response->printf("{\"total\":%llu}", cardCapacity);
    }
    request->send(response);
  });

  // SD LIST
  server.on("/sdlist", HTTP_GET, [](AsyncWebServerRequest *request) {
    while ((millis() - lastRequestTimemillis < requestIntervalmillis) && !isMeasuring) {
    }
    lastRequestTimemillis = millis();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"files\":[");

    SdFile dir;
    if (dir.open("/", O_RDONLY)) {
      SdFile file;
      bool first = true;
      while (file.openNext(&dir, O_RDONLY)) {
        if (!file.isDir()) {
          if (!first) response->print(",");
          char fileName[32];
          file.getName(fileName, sizeof(fileName));
          response->printf("{\"name\":\"%s\",\"size\":%lu}", fileName, file.fileSize());
          first = false;
        }
        file.close();
      }
      dir.close();
    }

    response->print("]}");
    request->send(response);
  });

  // SD UPLOAD
  server.on(
    "/sdupload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "File uploaded");  // 최신 SD 정보 반환
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        SdFile file;
        if (!file.open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
          DEBUG_PRINTLN(F("Failed to open file for writing on SD"));
          return;
        }
        file.close();
      }
      SdFile file;
      if (file.open(filename.c_str(), O_WRONLY | O_APPEND)) {
        file.write(data, len);
        file.close();
      }
  });

  // SD DELETE
  server.on("/sddelete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("filename", true)) {
      String filename = request->getParam("filename", true)->value();

      // currentlySavingFile과 비교하여 동일한 파일이 요청되었을 때 거부
      if (filename == "/" + currentlysavingFile) {
        request->send(403, "text/plain", "Cannot delete the currently saving file");
        return;
      }

      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }

      if (sd.exists(filename.c_str())) {
        if (sd.remove(filename.c_str())) {
          request->send(200, "text/plain", "File deleted");  // 최신 SD 정보 반환
        } else {
          request->send(500, "text/plain", "Failed to delete file on SD");
        }
      } else {
        request->send(404, "text/plain", "File not found on SD");
      }
    } else {
      request->send(400, "text/plain", "Bad Request: Missing filename");
    }
  });

  // SD DOWNLOAD
  server.on("/sddownload", HTTP_GET, [](AsyncWebServerRequest *request) {
    while(isSDbusy){
    }

    isSDbusy = true;

    if (request->hasParam("filename")) {
      String filename = request->getParam("filename")->value();

      // currentlySavingFile과 비교하여 동일한 파일이 요청되었을 때 거부
      if (filename == "/" + currentlysavingFile) {
        request->send(403, "text/plain", "Cannot download the currently saving file");
        return;
      }

      // 경로 보장
      if (!filename.startsWith("/")) {
        filename = "/" + filename;
      }

      while (isMeasuring) {
      }

      // 파일 존재 여부 확인
      if (sd.exists(filename.c_str())) {
        SdFile file;
        if (file.open(filename.c_str(), O_RDONLY)) {
          DEBUG_PRINTLN(F("File exists and opened. Streaming file..."));

          // 파일 크기 가져오기
          size_t fileSize = file.fileSize();

          // 파일 데이터를 수동으로 스트리밍
          request->send(
            "application/octet-stream", fileSize,
            [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
              if (index >= file.fileSize()) {
                file.close();  // 스트리밍 완료 시 파일 닫기
                return 0;
              }

              // 읽을 바이트 수 계산
              size_t bytesToRead = min(maxLen, file.fileSize() - index);

              // 파일을 읽기 전에 isMeasuring을 확인하여 대기
              while (isMeasuring) {
                return 0;  // isMeasuring이 true일 경우 현재 스트리밍을 잠시 멈추고 0을 반환하여 클라이언트에게 잠시 대기하도록 요청
              }

              // 파일 데이터를 버퍼로 읽기
              file.read(buffer, bytesToRead);
              return bytesToRead;  // 읽은 데이터 크기 반환
            });

          file.close();

        } else {
          DEBUG_PRINTLN(F("Failed to open file for streaming."));
          request->send(500, "text/plain", "Failed to open file on SD");
        }
      } else {
        DEBUG_PRINTLN(F("File not found on SD card."));
        request->send(404, "text/plain", "File not found on SD");
      }

      isSDbusy = false;

    } else {
      request->send(400, "text/plain", "Bad Request: Missing filename");
    }
  });
  
}
