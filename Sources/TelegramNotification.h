#pragma once

#include <string>
#include <json/value.h>

class TelegramNotification
{
private:

  bool enabled_ = false;

  int timeOut_;

  std::string url_;

  Json::Value bodyTemplate_ = Json::objectValue;

  TelegramNotification();

public:

  static TelegramNotification& Instance();

  void SendMessage(const Json::Value& message);
};