// OTAUpdateHandler.h
#ifndef OTA_UPDATER_HANDLER_H
#define OTA_UPDATER_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <Updater.h>

class OTAUpdateHandler {
private:
    AsyncWebServer* server;
    size_t update_content_len;
    String update_status;
    int last_progress_percentage;

public:
    OTAUpdateHandler(AsyncWebServer* server);
    void setup();
    void processUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
};

#endif