#include "Notification.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>
#include <Toolbox.h>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>


constexpr const char *ERROR_MESSAGE = "ErrorMessage";
constexpr const char *ERROR_DETAIL = "ErrorDetail";

class Notification::INotification : public boost::noncopyable
{
protected:
  bool enabled_ = false;

  int timeOut_;

  std::string url_;

  std::string authorization_;

  Json::Value bodyTemplate_ = Json::objectValue;

public:
  INotification()
  {
  }

  INotification(const std::string &name)
  {
    OrthancPlugins::OrthancConfiguration configuration;
    OrthancPlugins::OrthancConfiguration saola, Notification, bodyTemplate;
    configuration.GetSection(saola, "Saola");
    if (saola.IsSection(name))
    {
      this->enabled_ = true;
      saola.GetSection(Notification, name);
      Notification.GetSection(bodyTemplate, "BodyTemplate");
      this->url_ = Notification.GetStringValue("Url", "");
      this->authorization_ = Notification.GetStringValue("Authorization", "");
      this->timeOut_ = Notification.GetIntegerValue("Timeout", 60);
      this->bodyTemplate_ = bodyTemplate.GetJson();
    }
  }

  bool IsEnabled() const
  {
    return this->enabled_;
  }

  virtual ~INotification()
  {
  }

  virtual void SendMessage(const Json::Value &content) = 0;
};

class Notification::SimpleNotification : public Notification::INotification
{
public:
  SimpleNotification() : INotification("SimpleNotification")
  {
  }

  virtual void SendMessage(const Json::Value &content) ORTHANC_OVERRIDE
  {
    if (!this->enabled_)
    {
      LOG(INFO) << "[SimpleNotification] Not Enabled";
      return;
    }

    boost::thread t([=]()
                  {
    try
    {
      OrthancPlugins::HttpClient client;
      client.SetUrl(this->url_);
      client.SetMethod(OrthancPluginHttpMethod_Post);
      client.AddHeader("Content-Type", "application/json");
      if (!this->authorization_.empty())
      {
        client.AddHeader("Authorization", this->authorization_);
      }

      client.SetTimeout(this->timeOut_);

      Json::Value body;
      body.copy(this->bodyTemplate_);
      body["detail"] = "";
      body["error"] = "";
      if (content.isMember(ERROR_DETAIL))
      {
        body["detail"] = content[ERROR_DETAIL];
      }

      if (content.isMember(ERROR_MESSAGE))
      {
        body["error"] = content[ERROR_MESSAGE];
      }

      std::string bodyStr;
      Orthanc::Toolbox::WriteFastJson(bodyStr, body);
      client.SetBody(bodyStr);
      client.Execute();
    }
    catch (std::exception& e)
    {
      LOG(ERROR) << "[SimpleNotification] ERROR Got error: " << e.what();
    }
    catch (...)
    {
    } });

    t.detach();
  }
};

class Notification::TelegramNotification : public Notification::INotification
{
public:
  TelegramNotification() : INotification("Telegram")
  {
  }
  virtual void SendMessage(const Json::Value &content) ORTHANC_OVERRIDE
  {
    if (!this->enabled_)
    {
      LOG(INFO) << "[Telegram] Not Enable";
      return;
    }

    boost::thread t([=]()
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
      LOG(ERROR) << "[Telegram] ERROR Got error: " << e.what();
    }
    catch (...)
    {
    } });

    t.detach();
  }
};

Notification::Notification()
{
  notifications_.push_back(std::make_unique<Notification::SimpleNotification>());
  notifications_.push_back(std::make_unique<Notification::TelegramNotification>());
}

Notification &Notification::Instance()
{
  static Notification instance;
  return instance;
}

void Notification::SendMessage(const Json::Value &content)
{
  for (auto &notification : notifications_)
  {
    if (notification->IsEnabled())
    {
      notification->SendMessage(content);
    }
  }
}
