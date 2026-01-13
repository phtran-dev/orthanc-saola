#pragma once

#include <string>

struct StableEventDTOCreate
{
  /* data */
  std::string iuid_;
  std::string resource_id_;
  std::string resouce_type_;
  std::string app_id_;
  std::string app_type_;
  int         delay_ = 0;
  std::string patient_birth_date_;
  std::string patient_id_;
  std::string patient_name_;
  std::string patient_sex_;
  std::string accession_number_; 
  
  StableEventDTOCreate()
  {}
};
