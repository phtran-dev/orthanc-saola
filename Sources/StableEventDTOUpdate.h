#pragma once

struct StableEventDTOUpdate
{
  /* data */
  int id_;
  const char* failed_reason_;
  int retry_;
  StableEventDTOUpdate(int id, const char* failed_reason, int retry) :
      id_(id), failed_reason_(failed_reason), retry_(retry)
  {}
};
