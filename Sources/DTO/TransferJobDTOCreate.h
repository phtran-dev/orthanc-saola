#pragma once

#include <string>

struct TransferJobDTOCreate
{
  /* data */
  std::string id_;
  std::string owner_id_;

  int64_t queue_id_;

  TransferJobDTOCreate(const std::string& id, const std::string& owner_id, int64_t queue_id) :
    id_(id), owner_id_(owner_id), queue_id_(queue_id)
  {}
};