#pragma once

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>

struct StableEventDTOGet
{
  /* data */
  int64_t     id_;
  std::string iuid_;
  std::string resource_id_;
  std::string resource_type_;
  std::string app_id_;
  int         delay_sec_ = 0;
  int         retry_;
  std::string failed_reason_;
  std::string creation_time_;

  StableEventDTOGet()
  {}

  StableEventDTOGet(int64_t id,
                    std::string&& iuid,
                    std::string&& resource_id,
                    std::string&& resource_type,
                    std::string&& app_id,
                    int delay_sec,
                    int retry,
                    std::string&& failed_reason,
                    std::string&& creation_time) :
    id_(id),
    iuid_(iuid),
    resource_id_(resource_id),
    resource_type_(resource_type), 
    app_id_(app_id),
    delay_sec_(delay_sec), 
    retry_(retry), 
    failed_reason_(failed_reason), 
    creation_time_(creation_time)
  {}

  void ToJson(Json::Value& json) const
  {
    json["id"] = id_;
    json["iuid"] = iuid_;
    json["resourceId"] = resource_id_;
    json["resourceType"] = resource_type_;
    json["app_id"] = app_id_;
    json["delaySec"] = delay_sec_;
    json["retry"] = retry_;
    json["failedReason"] = failed_reason_;
    json["creationTime"] = creation_time_;
  }
};
