#pragma once

#include "TimeUtil.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>
#include <sstream>

struct StableEventDTOGet
{
  /* data */
  int64_t id_;
  std::string iuid_;
  std::string resource_id_;
  std::string resource_type_;
  std::string app_id_;
  std::string app_type_;
  int delay_sec_ = 0;
  int retry_;
  std::string failed_reason_;
  std::string last_updated_time_;
  std::string creation_time_;

  StableEventDTOGet()
  {
  }

  StableEventDTOGet(int64_t id,
                    std::string &&iuid,
                    std::string &&resource_id,
                    std::string &&resource_type,
                    std::string &&app_id,
                    std::string &&app_type,
                    int delay_sec,
                    int retry,
                    std::string &&failed_reason,
                    std::string &&last_updated_time,
                    std::string &&creation_time) : id_(id),
                                                   iuid_(iuid),
                                                   resource_id_(resource_id),
                                                   resource_type_(resource_type),
                                                   app_id_(app_id),
                                                   app_type_(app_type),
                                                   delay_sec_(delay_sec),
                                                   retry_(retry),
                                                   failed_reason_(failed_reason),
                                                   last_updated_time_(last_updated_time),
                                                   creation_time_(creation_time)
  {
  }

  void ToJson(Json::Value &json) const
  {
    json["id"] = id_;
    json["iuid"] = iuid_;
    json["resourceId"] = resource_id_;
    json["resourceType"] = resource_type_;
    json["app_id"] = app_id_;
    json["app_type"] = app_type_;
    json["delaySec"] = delay_sec_;
    json["retry"] = retry_;
    json["failedReason"] = failed_reason_;
    json["lastUpdatedTime"] = last_updated_time_;
    json["creationTime"] = creation_time_;
    json["now"] = boost::posix_time::to_iso_string(Saola::GetNow());
  }

  std::string ToJsonString() const
  {
    std::stringstream ss;
    ss << "StableEventDTOGet {id=" << this->id_ << ", iuid=" << this->iuid_ << ", resource_id=" << this->resource_id_
       << ", app_id_=" << this->app_id_ << ", app_type_=" << this->app_type_ << ", resource_type=" << this->resource_type_ 
       << ", retry_=" << this->retry_ << ", delay_sec=" << delay_sec_ << ", last_updated_time=" << this->last_updated_time_ << ", creation_time=" 
       << this->creation_time_ << ", now=" << boost::posix_time::to_iso_string(Saola::GetNow()) 
       << ", elapsed=" << Saola::Elapsed(this->creation_time_) << "}";
    return ss.str();
  }
};
