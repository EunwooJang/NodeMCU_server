#ifdef ENABLE_EMAIL

#include "email.h"

// 전역 변수로 선언한 SMTP 세션과 설정 객체
SMTPSession smtp;
Session_Config config;


// 일반 이메일 전송 함수
void sendEmail(const char* subject, const char* messages) {

  // SMTP 서버 정보 설정
  config.server.host_name = "smtp.gmail.com";
  config.server.port = 465;
  config.login.email = "sender@gmail.com";
  config.login.password = "16_digit_password";
  config.time.ntp_server = F("pool.ntp.org");

  // SMTP 세션 연결 시도 (초기화 시 한번 연결을 시도)
  if (!smtp.connect(&config)) {
    DEBUG_PRINTLN("SMTP 연결 실패");
    // 연결 실패 시 필요한 처리 추가 (예: 재시도, 오류 플래그 설정 등)
  } else {
    DEBUG_PRINTLN("SMTP 연결 성공");
  }

  // 이메일 메시지 생성 및 구성
  SMTP_Message message;
  message.sender.name = "NodeMCU v1.1.1";
  message.sender.email = "sender@gmail.com";
  message.subject = subject;
  message.addRecipient("recipient", "recipient@gmail.com");
  message.text.content = messages;
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  // 이메일 전송
  if (MailClient.sendMail(&smtp, &message)) {
    DEBUG_PRINTLN("이메일 전송 성공");
  } else {
    DEBUG_PRINTLN("이메일 전송 실패");
  }

  // 메시지 및 결과 클리어
  message.clear();
  smtp.sendingResult.clear();
}

#endif  // ENABLE_EMAIL
