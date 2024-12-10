#pragma once

struct StableEventDTOCreate
{
  /* data */
  const char* iuid_;
  const char* resource_id_;
  const char* resouce_type_;
  const char* app_id_;
  int         delay_ = 0;
  StableEventDTOCreate()
  {}
};
