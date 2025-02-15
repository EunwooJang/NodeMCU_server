#include "email.h"

// SMTP 세션 설정
SMTPSession smtp;
Session_Config config;

void sendEmail(const char* subject, const char* messages) {
  // SMTP 서버 정보 설정
  config.server.host_name = "smtp.gmail.com";
  config.server.port = 465;
  config.login.email = "user@gmail.com";
  config.login.password = "password";
  config.time.ntp_server = F("pool.ntp.org");

  // SMTP 세션 연결
  if (!smtp.connect(&config)) return;

  // 이메일 메시지 생성
  SMTP_Message message;

  message.sender.name = "NodeMCU v1.1.0";
  message.sender.email = "sender@gmail.com";
  message.subject = subject;
  message.addRecipient("Recipient", "recipient@gmail.com");
  message.text.content = messages;
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  // 이메일 전송
  MailClient.sendMail(&smtp, &message);
  smtp.sendingResult.clear();
}
