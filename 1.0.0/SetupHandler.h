#ifndef SETUPHANDLER_H
#define SETUPHANDLER_H

#include <ESPAsyncWebServer.h>
#include <SdFat.h>

extern const int chipSelect;  // 전역 변수 선언
extern SdFat sd;             // SD 카드 객체 전역 변수 선언

void handleSetupRoutes(AsyncWebServer& server);

#endif
