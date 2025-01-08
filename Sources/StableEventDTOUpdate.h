#pragma once

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

struct StableEventDTOUpdate
{
  /* data */
  int64_t id_;
  const char* failed_reason_;
  int retry_;
  StableEventDTOUpdate(int64_t id, const char* failed_reason, int retry) :
      id_(id), failed_reason_(failed_reason), retry_(retry)
  {}
};
