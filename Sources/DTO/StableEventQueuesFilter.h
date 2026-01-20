#pragma once

#include <string>

struct StableEventQueuesFilter
{
  std::string patient_id_;
  std::string patient_name_;
  std::string accession_number_;
  std::string owner_id_;

  bool IsEmpty() const
  {
    return patient_id_.empty() && patient_name_.empty() && accession_number_.empty();
  }
};
