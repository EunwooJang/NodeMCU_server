// OTAUpdateHandler.cpp
#include "OTAUpdaterHandler.h"
#include <FS.h>

OTAUpdateHandler::OTAUpdateHandler(AsyncWebServer* server) {
    this->server = server;
    update_content_len = 0;
    update_status = "";
    last_progress_percentage = 0;
}

void OTAUpdateHandler::setup() {
    // OTA 업데이트를 위한 /update 경로 설정
    server->on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/update.html", "text/html");
    });

    server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {},
               [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
                   this->processUpdate(request, filename, index, data, len, final);
               });
}

void OTAUpdateHandler::processUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        update_status = "Update started!";
        Serial.println("Update started");
        update_content_len = request->contentLength();

        Update.runAsync(true);
        if (!Update.begin(update_content_len, U_FLASH)) {
            Update.printError(Serial);
            return;
        }
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    } else {
        int current_progress = (Update.progress() * 100) / Update.size();
        if (current_progress - last_progress_percentage >= 10) {
            last_progress_percentage = current_progress;
            update_status = "Progress: " + String(current_progress) + "%";
            Serial.println(update_status);
        }
    }

    if (final) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots...");
        response->addHeader("Refresh", "5");
        response->addHeader("Location", "/");
        request->send(response);

        if (!Update.end(true)) {
            Update.printError(Serial);
        } else {
            update_status = "Done!";
            Serial.println("Update complete");

            // 삭제할 업데이트 파일이 존재하는 경우 삭제
            if (SPIFFS.exists("/update.bin")) {
                if (SPIFFS.remove("/update.bin")) {
                    Serial.println("Update file deleted successfully");
                } else {
                    Serial.println("Failed to delete update file");
                }
            }

            Serial.flush();
            ESP.restart();
        }
    }
}
