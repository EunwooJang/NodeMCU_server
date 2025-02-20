#ifndef WIFICONNECT_H
#define WIFICONNECT_H

#include "global.h"

// 웹서버에서 사용할 SdFat 객체. 이는 메인에서 선언됨
extern SdFat sd;

// 와이파이 연결 함수
void Wificonnect();

#endif
