#include "TelegramNotification.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>
#include <Toolbox.h>
#include <thread>

TelegramNotification::TelegramNotification()
{
  OrthancPlugins::OrthancConfiguration configuration;
  OrthancPlugins::OrthancConfiguration saola, telegram, bodyTemplate;
  configuration.GetSection(saola, "Saola");
  if (saola.IsSection("Telegram"))
  {
    this->enabled_ = true;
    saola.GetSection(telegram, "Telegram");
    telegram.GetSection(bodyTemplate, "BodyTemplate");
    this->url_ = telegram.GetStringValue("Url", "");
    this->timeOut_ = telegram.GetIntegerValue("TimeOut", 60);
    this->bodyTemplate_ = bodyTemplate.GetJson();
  }
}

void TelegramNotification::SendMessage(const Json::Value &content)
{
  if (!this->enabled_)
  {
    LOG(INFO) << "[Telegram] Not Enable";
    return;
  }

  std::thread t([=]()
                {
    try
    {
      OrthancPlugins::HttpClient client;
      client.SetUrl(this->url_);
      client.SetMethod(OrthancPluginHttpMethod_Post);
      client.AddHeader("Content-Type", "application/json");
      client.SetTimeout(this->timeOut_);

      Json::Value body;
      body.copy(this->bodyTemplate_);
      body["text"]["Content"] = content;

      std::string bodyStr;
      Orthanc::Toolbox::WriteStyledJson(bodyStr, body);
      client.SetBody(bodyStr);
      client.Execute();
    }
    catch (std::exception& e)
    {
      LOG(ERROR) << "[Telegram] Got error: " << e.what();
    }
    catch (...)
    {
    } });

  t.detach();
}

TelegramNotification &TelegramNotification::Instance()
{
  static TelegramNotification instance;
  return instance;
}