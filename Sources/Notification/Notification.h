#pragma once

#include <list>

#include <json/value.h>

class Notification
{
private:
  class INotification;
  class TelegramNotification;
  class SimpleNotification;

  std::list<std::unique_ptr<INotification>> notifications_;

  Notification();

public:

  static constexpr const char *ERROR_MESSAGE = "ErrorMessage";

  static constexpr const char *ERROR_DETAIL = "ErrorDetail";

  static Notification& Instance();

  void SendMessage(const Json::Value &content);

};