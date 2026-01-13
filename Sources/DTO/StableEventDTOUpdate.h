#pragma once

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

struct StableEventDTOUpdate
{
  /* data */
  int64_t id_;
  const char* failed_reason_;
  int retry_;
  const char* last_updated_time_;
  const char* next_scheduled_time_;
  const char* status_;  // Processing, Pending
  StableEventDTOUpdate(int64_t id, const char* failed_reason, int retry, const char* last_updated_time, const char* next_scheduled_time, const char* status = "PENDING") :
      id_(id), failed_reason_(failed_reason), retry_(retry), last_updated_time_(last_updated_time), next_scheduled_time_(next_scheduled_time), status_(status)
  {}
};
