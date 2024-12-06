#pragma once

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>

struct FailedJobDTOGet
{
  /* data */
  std::string id_;
  std::string content_;
  int         retry_;
  std::string last_updated_time_;
  std::string creation_time_;

  FailedJobDTOGet()
  {}

  FailedJobDTOGet(std::string&& id,
                  std::string&& content,
                  int retry,
                  std::string&& last_updated_time,
                  std::string&& creation_time) :
    id_(id),
    content_(content), 
    retry_(retry),
    last_updated_time_(last_updated_time),
    creation_time_(creation_time)
  {}

  void ToJson(Json::Value& json) const
  {
    json["id"] = id_;
    json["content"] = content_;
    json["retry"] = retry_;
    json["last_updated_time"] = last_updated_time_;
    json["creationTime"] = creation_time_;
  }

};