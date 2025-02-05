#include <ESP8266WiFi.h>
#include <FS.h>
#include "SetupHandler.h"
#include <ArduinoJson.h>

// 전역 변수 정의
const int chipSelect = 15;
uint64_t totalUsedBytes = 0;

// SD 카드 사용량 업데이트 함수
void updateSDUsedBytes() {
  totalUsedBytes = 0;
  SdFile dir, file;
  if (dir.open("/")) {
      while (file.openNext(&dir, O_RDONLY)) {
          if (!file.isDir()) {
              totalUsedBytes += file.fileSize();
          }
          file.close();
      }
      dir.close();
  }
  // Serial.println(totalUsedBytes);
  
  if (sd.card()) {
        uint64_t cardSize = sd.card()->sectorCount();
        uint64_t cardCapacity = cardSize * 512;
        if (cardCapacity > 0) {
            float usage = (totalUsedBytes * 100.0) / cardCapacity;
            // Serial.printf("SD Usage: %.5f%%\n", usage); // Usage 디버깅 메시지
        }
    }
}

// SPIFFS 용량 정보 반환
String getSPIFFSInfo() {
    FSInfo fs_info;
    SPIFFS.info(fs_info);

    DynamicJsonDocument doc(256);
    doc["total"] = fs_info.totalBytes;
    doc["used"] = fs_info.usedBytes;
    doc["percentage"] = fs_info.totalBytes > 0 ? (fs_info.usedBytes * 100.0) / fs_info.totalBytes : 0;

    String json;
    serializeJson(doc, json);
    return json;
}

// 파일 목록 JSON으로 반환 (SPIFFS)
String getFileList() {
    DynamicJsonDocument doc(1024);
    JsonArray files = doc.createNestedArray("files");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
        JsonObject file = files.createNestedObject();
        file["name"] = dir.fileName();
        file["size"] = dir.fileSize();
    }

    String json;
    serializeJson(doc, json);
    return json;
}

// SD 용량 정보를 JSON으로 반환
String getSDInfo() {
    DynamicJsonDocument doc(256);

    if (!sd.card()) {
        doc["total"] = 0;
        doc["used"] = 0;
        doc["percentage"] = 0;
    } else {
        uint64_t cardSize = sd.card()->sectorCount(); // 카드 크기 (섹터 수)
        uint64_t cardCapacity = cardSize * 512;       // 카드 크기 (바이트)

        if (cardCapacity == 0) {
            doc["total"] = 0;
            doc["used"] = 0;
            doc["percentage"] = 0;
        } else {
            doc["total"] = cardCapacity;
            doc["used"] = totalUsedBytes;
            doc["percentage"] = (totalUsedBytes * 100.0) / cardCapacity; // 정확한 퍼센트 계산
        }
    }

    String json;
    serializeJson(doc, json);
    return json;
}

// SD 카드의 파일 목록을 JSON으로 반환
String getSDFileList() {
    DynamicJsonDocument doc(1024);
    JsonArray files = doc.createNestedArray("files");

    SdFile dir, file;
    if (dir.open("/")) {
        while (file.openNext(&dir, O_RDONLY)) {
            char fileName[50];
            file.getName(fileName, 50);
            uint64_t fileSize = file.isDir() ? 0 : file.fileSize();

            if (fileSize > 0) { // 0바이트 파일 제외
                JsonObject fileObj = files.createNestedObject();
                fileObj["name"] = String(fileName);
                fileObj["size"] = fileSize;
                Serial.println(String(fileName));
            }

            file.close();
        }
        dir.close();
    }

    String json;
    serializeJson(doc, json);
    return json;
}

void handleSetupRoutes(AsyncWebServer& server) {

    // Main setup page
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/setup.html", "text/html");
    });

    // SPIFFS LIST
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = getFileList();
        request->send(200, "application/json", json);
    });

    // SPIFFS INFO
    server.on("/spiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = getSPIFFSInfo();
        request->send(200, "application/json", json);
    });

    // SPIFFS upload
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "File uploaded successfully");
    }, 
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            File file = SPIFFS.open("/" + filename, "w");
            if (!file) {
                Serial.println("Failed to open file for writing");
                return;
            }
            file.close();
        }
        File file = SPIFFS.open("/" + filename, "a");
        if (file) {
            file.write(data, len);
            file.close();
        }
    });

    // SPIFFS delete
    server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("filename", true)) {
            String filename = request->getParam("filename", true)->value();
            if (SPIFFS.exists(filename)) {
                SPIFFS.remove(filename);
                request->send(200, "text/plain", "File deleted");
            } else {
                request->send(404, "text/plain", "File not found");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // SPIFFS download
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("filename")) {
            String filename = request->getParam("filename")->value();
            if (SPIFFS.exists(filename)) {
                request->send(SPIFFS, filename, "application/octet-stream");
            } else {
                request->send(404, "text/plain", "File not found");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // SD INFO
    server.on("/sdinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = getSDInfo();
        request->send(200, "application/json", json);
    });

    // SD LIST
    server.on("/sdlist", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = getSDFileList();
        request->send(200, "application/json", json);
    });

    // SD UPLOAD
    server.on("/sdupload", HTTP_POST, [](AsyncWebServerRequest *request) {
      String json = getSDInfo(); // 최신 정보 생성
      request->send(200, "application/json", json); // 최신 SD 정보 반환
    }, 
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            SdFile file;
            if (!file.open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
                Serial.println("Failed to open file for writing on SD");
                return;
            }
            file.close();
        }
        SdFile file;
        if (file.open(filename.c_str(), O_WRONLY | O_APPEND)) {
            file.write(data, len);
            file.close();
        }
        if (final) {
            updateSDUsedBytes(); // 업로드 후 SD 용량 갱신
        }
    });

    // SD DELETE
    server.on("/sddelete", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("filename", true)) {
            String filename = request->getParam("filename", true)->value();
            if (!filename.startsWith("/")) {
                filename = "/" + filename;
            }

            if (sd.exists(filename.c_str())) {
                if (sd.remove(filename.c_str())) {
                    updateSDUsedBytes(); // 삭제 후 SD 용량 갱신
                    String json = getSDInfo(); // 최신 정보 생성
                    request->send(200, "application/json", json); // 최신 SD 정보 반환
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
        if (request->hasParam("filename")) {
            String filename = request->getParam("filename")->value();

            // 경로 보장
            if (!filename.startsWith("/")) {
                filename = "/" + filename;
            }

            Serial.printf("Checking if file exists: %s\n", filename.c_str());

            // 파일 존재 여부 확인
            if (sd.exists(filename.c_str())) {
                SdFile file;
                if (file.open(filename.c_str(), O_RDONLY)) {
                    Serial.println("File exists and opened. Streaming file...");

                    // 파일 크기 가져오기
                    size_t fileSize = file.fileSize();

                    // 파일 데이터를 수동으로 스트리밍
                    request->send(
                        "application/octet-stream", fileSize,
                        [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
                            if (index >= file.fileSize()) {
                                file.close(); // 스트리밍 완료 시 파일 닫기
                                return 0;
                            }

                            // 읽을 바이트 수 계산
                            size_t bytesToRead = min(maxLen, file.fileSize() - index);
                            file.read(buffer, bytesToRead); // 데이터를 버퍼로 읽기
                            return bytesToRead; // 읽은 데이터 크기 반환
                        }
                    );
                    
                    file.close();

                } else {
                    Serial.println("Failed to open file for streaming.");
                    request->send(500, "text/plain", "Failed to open file on SD");
                }
            } else {
                Serial.println("File not found on SD card.");
                request->send(404, "text/plain", "File not found on SD");
            }
        } else {
            request->send(400, "text/plain", "Bad Request: Missing filename");
        }
    });
    

}
