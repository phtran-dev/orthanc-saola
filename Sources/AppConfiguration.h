#pragma once

#include <orthanc/OrthancCPlugin.h>

#include <string>
#include <map>

#include <json/value.h>

struct AppConfiguration
{
  std::string id_;

  bool enable_;

  std::string type_;

  unsigned int delay_ = 0;

  int timeOut_ = 60;

  std::string url_;

  std::string authentication_;

  OrthancPluginHttpMethod method_ = OrthancPluginHttpMethod_Post;

  Json::Value fieldMapping_;

  Json::Value fieldValues_;

  std::string luaCallback_ = "";

  AppConfiguration()
  {
  }

  void ToJson(Json::Value &json) const
  {
    json["id_"] = this->id_;
    json["enable_"] = this->enable_;
    json["type_"] = this->type_;
    json["delay_"] = this->delay_;
    json["timeOut_"] = this->timeOut_;
    json["url_"] = this->url_;
    json["authentication_"] = this->authentication_;
    json["method_"] = this->method_;
    json["fieldMapping_"] = this->fieldMapping_;
    json["fieldValues_"] = this->fieldValues_;
    json["luaCallback_"] = this->luaCallback_;
  }
};