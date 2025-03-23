#pragma once

#include <string>
#include <json/value.h>

#include <boost/noncopyable.hpp>

class BaseNotification : public boost::noncopyable
{
protected:

  bool enabled_ = false;

  int timeOut_;

  std::string url_;

  std::string authorization_;

  Json::Value bodyTemplate_ = Json::objectValue;

  BaseNotification();

  virtual ~BaseNotification();

public:

  virtual void SendMessage(const Json::Value& message) = 0;
};