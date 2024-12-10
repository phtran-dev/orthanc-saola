#pragma once

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>

struct TransferJobDTOGet
{
  /* data */
  std::string id_;
  int64_t queue_id_;
  std::string last_updated_time_;
  std::string creation_time_;

  TransferJobDTOGet()
  {}

  TransferJobDTOGet(std::string&& id,
                    ino64_t queue_id, 
                    std::string&& last_updated_time,
                    std::string&& creation_time) :
    id_(id),
    queue_id_(queue_id),
    last_updated_time_(last_updated_time),
    creation_time_(creation_time)
  {}

  void ToJson(Json::Value& json) const
  {
    json["id"] = id_;
    json["lastUpdatedTime"] = last_updated_time_;
    json["creationTime"] = creation_time_;
  }

};