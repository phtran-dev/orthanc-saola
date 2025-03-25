#pragma once

#include <orthanc/OrthancCPlugin.h>

#include <string>
#include <map>

#include <json/value.h>

struct AppConfiguration
{
  std::string id_;

  bool enable_ = true;

  std::string type_ = "";

  unsigned int delay_ = 0;

  std::string url_ = "";

  std::string authentication_ = "";

  OrthancPluginHttpMethod method_ = OrthancPluginHttpMethod_Post;

  int timeOut_ = 60;

  bool fieldMappingOverwrite = false;

  Json::Value fieldMapping_;

  Json::Value fieldValues_;

  std::string luaCallback_ = "";

  AppConfiguration()
  {
  }

  void ToJson(Json::Value &json) const
  {
    json["Id"] = this->id_;
    json["Enable"] = this->enable_;
    json["Type"] = this->type_;
    json["Delay"] = this->delay_;
    json["Url"] = this->url_;
    json["Authentication"] = this->authentication_;
    json["Method"] = this->method_;
    json["Timeout"] = this->timeOut_;
    json["FieldMappingOverwrite"] = this->fieldMappingOverwrite;
    json["FieldMapping"] = this->fieldMapping_;
    json["FieldValues"] = this->fieldValues_;
    json["LuaCallback"] = this->luaCallback_;
  }
};