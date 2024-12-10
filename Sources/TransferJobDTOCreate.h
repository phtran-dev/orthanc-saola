#pragma once

#include <string>

struct TransferJobDTOCreate
{
  /* data */
  std::string id_;

  int64_t queue_id_;

  TransferJobDTOCreate(const std::string& id, int64_t queue_id) :
    id_(id), queue_id_(queue_id)
  {}
};