#pragma once

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>
#include <sstream>

struct TransferJobDTOGet
{
  /* data */
  std::string id_;
  int64_t queue_id_;
  std::string last_updated_time_;
  std::string creation_time_;

  TransferJobDTOGet()
  {
  }

  TransferJobDTOGet(std::string &&id,
                    ino64_t queue_id,
                    std::string &&last_updated_time,
                    std::string &&creation_time) : id_(id),
                                                   queue_id_(queue_id),
                                                   last_updated_time_(last_updated_time),
                                                   creation_time_(creation_time)
  {
  }

  void ToJson(Json::Value &json) const
  {
    json["id"] = id_;
    json["lastUpdatedTime"] = last_updated_time_;
    json["creationTime"] = creation_time_;
  }

  std::string ToJsonString() const
  {
    std::stringstream ss;
    ss << "TransferJobDTOGet {id=" << this->id_ << ", queue_id_=" << this->queue_id_ << ", last_updated_time_=" << this->last_updated_time_ << ", creation_time_=" << this->creation_time_ << "}";
    return ss.str();
  }
};