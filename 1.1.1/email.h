#ifndef EMAIL_H
#define EMAIL_H

#include "global.h"

#ifdef ENABLE_EMAIL
  // 실제 이메일 전송 함수 선언 (구현은 email.cpp에 있음)
  void sendEmail(const char* subject, const char* message);

 #else
  // 이메일 기능을 사용하지 않을 경우, 빈 inline 함수로 정의
  inline void sendEmail(const char* subject, const char* message) { }

#endif

#endif
