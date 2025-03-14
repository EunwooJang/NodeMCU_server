#include "WebSocketHandler.h"

// 버전 정보 (HTML 등에서 공유)
const char *version = "1.1.0";

// Slave 아두이노 및 센서 관련 설정
int temp_slave_Amount = 4;   // slave 아두이노 개수
int temp_sensor_Amount = 5;  // 각 아두이노당 센서 개수

int magnetic_slave_Amount = 2;   // slave 아두이노 개수
int magnetic_sensor_Amount = 1;  // 각 아두이노당 센서 개수

int cur_index = 0;
bool new_client = false;
int client_n = 0;

int alive_temp_slave[] = {1, 0, 1, 0};
int alive_mag_slave[] = {0, 0};

int max_counts = 259200;

// 데이터 관련 변수 (HTML에 공유)
unsigned long acquisitiontimeInterval = 10000;  // 데이터 수집 주기 (ms)

// OTA 업데이트 과정에서 사용할 파일의 전체 크기를 저장하는 전역 변수
static size_t update_content_len = 0;
// 요청 속도 제한을 위한 변수
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 500;  // 최소 요청 간격 (밀리초 단위)

// LittleFS 용량 정보 반환
const char *getLittleFSInfo() {
  FSInfo fs_info;
  LittleFS.info(fs_info);
  static char json[64];
  snprintf(json, sizeof(json), "{\"total\":%u}", fs_info.totalBytes);
  return json;
}

// SD 용량 정보를 JSON으로 반환
const char *getSDInfo() {
  static char json[64];         // 기존과 동일한 크기 유지
  DynamicJsonDocument doc(64);  // JSON 문서 크기 제한

  if (!sd.card()) {
    doc["total"] = 0;
  } else {
    uint64_t cardSize = sd.card()->sectorCount();  // 섹터 수
    uint64_t cardCapacity = cardSize * 512;        // 바이트 단위 크기

    doc["total"] = (cardCapacity == 0) ? 0 : cardCapacity;
  }

  // JSON 데이터를 `json` 배열에 직렬화
  size_t len = serializeJson(doc, json, sizeof(json));

  // 직렬화가 실패했거나, 크기가 초과되었을 경우 대비
  if (len == 0 || len >= sizeof(json)) {
    snprintf(json, sizeof(json), "{\"total\":0}");
  }

  return json;
}


void setupWebSocket(AsyncWebServer &server, AsyncWebSocket &ws) {
  // HTTP 라우트 등록
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTime < requestInterval || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTime = millis();
    request->send(LittleFS, "/index.html", "text/html");
  });

  // favicon 등록 : 존재하면 제공, 없으면 204(No Content) 응답
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/favicon.ico")) {
      request->send(LittleFS, "/favicon.ico", "image/x-icon");
    } else {
      request->send(204);
    }
  });

  // WebSocket 이벤트 핸들러 등록 ("/ws" 경로는 ws 객체가 생성될 때 지정됨)
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      new_client = true;
      client_n++;
      Serial.println(F("WebSocket Client Connected"));
      Serial.println(client_n);

    } else if (type == WS_EVT_DISCONNECT) {
      client_n--;
      Serial.println(F("WebSocket Client Disconnected"));
      Serial.println(client_n);

    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->opcode == WS_TEXT) {
        char message[len + 1];
        memcpy(message, data, len);
        message[len] = '\0';  // 문자열 종료

        // 예시: 받은 메시지에 따라 동작 분기
        if (strcmp(message, "start") == 0) {
          isSaving = true;
          Serial.println(F("Starting saving process..."));
          // 저장 시작 로직 실행
        } else if (strcmp(message, "stop") == 0) {
          isSaving = false;
          Serial.println(F("Stopping saving process..."));
          // 저장 중지 로직 실행
        } else if (strcmp(message, "reset") == 0) {
        }
      }
    }
  });

  // GET 요청: 클라이언트가 /update 경로로 접속하면, update.html 페이지를 제공
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTime < requestInterval || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTime = millis();
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
        Serial.println(F("Update started"));
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
          Serial.println(F("Update complete"));
          Serial.flush();
          ESP.restart();
        }
      }
  });

  
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (millis() - lastRequestTime < requestInterval || isMeasuring) {
      request->send(200, "text/html", "<script>setTimeout(function(){ location.reload(); }, 1000);</script>Loading...");
      return;
    }
    lastRequestTime = millis();
    request->send(LittleFS, "/setup.html", "text/html");
  });


  // LittleFS LIST
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    while (millis() - lastRequestTime < requestInterval) {
    }
    lastRequestTime = millis();
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
    while (millis() - lastRequestTime < requestInterval) {
    }
    lastRequestTime = millis();
    request->send(200, "application/json", getLittleFSInfo());
  });

  // LittleFS UPLOAD
  server.on(
    "/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "File uploaded successfully");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        File file = LittleFS.open("/" + filename, "w");
        if (!file) {
          Serial.println(F("Failed to open file for writing"));
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
    while (millis() - lastRequestTime < requestInterval) {
    }
    lastRequestTime = millis();
    request->send(200, "application/json", getSDInfo());
  });

  // SD LIST
  server.on("/sdlist", HTTP_GET, [](AsyncWebServerRequest *request) {
    while (millis() - lastRequestTime < requestInterval) {
    }
    lastRequestTime = millis();
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
      String json = getSDInfo();                     // 최신 정보 생성
      request->send(200, "application/json", json);  // 최신 SD 정보 반환
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        SdFile file;
        if (!file.open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
          Serial.println(F("Failed to open file for writing on SD"));
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
          String json = getSDInfo();                     // 최신 정보 생성
          request->send(200, "application/json", json);  // 최신 SD 정보 반환
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
          Serial.println(F("File exists and opened. Streaming file..."));

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
          Serial.println(F("Failed to open file for streaming."));
          request->send(500, "text/plain", "Failed to open file on SD");
        }
      } else {
        Serial.println(F("File not found on SD card."));
        request->send(404, "text/plain", "File not found on SD");
      }

      isSDbusy = false;

    } else {
      request->send(400, "text/plain", "Bad Request: Missing filename");
    }
  });
}
