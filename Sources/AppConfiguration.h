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

  std::map<std::string, std::string> fieldMapping_;

  Json::Value fieldValues_;

  AppConfiguration()
  {
  }

  void clone(AppConfiguration& that) const
  {
    that.id_ = this->id_;
    that.enable_ = this->enable_;
    that.type_ = this->type_;
    that.delay_ = this->delay_;
    that.url_ = this->url_;
    that.method_ = this->method_;
    that.authentication_ = this->authentication_;
    for (const auto& m : this->fieldMapping_)
    {
      that.fieldMapping_[m.first] = m.second;
    }
    that.fieldValues_.copy(this->fieldValues_);
  }

};