#pragma once

#include "../TimeUtil.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <string>
#include <sstream>

struct StableEventDTOGet
{
  /* data */
  int64_t id_;
  std::string status_;
  std::string owner_id_;
  std::string patient_birth_date_;
  std::string patient_id_;
  std::string patient_name_;
  std::string patient_sex_;
  std::string accession_number_;
  std::string iuid_;
  std::string resource_id_;
  std::string resource_type_;
  std::string app_id_;
  std::string app_type_;
  int delay_sec_ = 0;
  int retry_;
  std::string failed_reason_;
  std::string next_scheduled_time_;
  std::string expiration_time_;
  std::string last_updated_time_;
  std::string creation_time_;

  StableEventDTOGet()
  {
  }

  StableEventDTOGet(int64_t id,
                    std::string &&status,
                    std::string &&owner_id,
                    std::string &&patient_birth_date,
                    std::string &&patient_id,
                    std::string &&patient_name,
                    std::string &&patient_sex,
                    std::string &&accession_number,
                    std::string &&iuid,
                    std::string &&resource_id,
                    std::string &&resource_type,
                    std::string &&app_id,
                    std::string &&app_type,
                    int delay_sec,
                    int retry,
                    std::string &&failed_reason,
                    std::string &&next_scheduled_time,
                    std::string &&expiration_time,
                    std::string &&last_updated_time,
                    std::string &&creation_time) : id_(id),
                                                   status_(status),
                                                   owner_id_(owner_id),
                                                   patient_birth_date_(patient_birth_date),
                                                   patient_id_(patient_id),
                                                   patient_name_(patient_name),
                                                   patient_sex_(patient_sex),
                                                   accession_number_(accession_number),
                                                   iuid_(iuid),
                                                   resource_id_(resource_id),
                                                   resource_type_(resource_type),
                                                   app_id_(app_id),
                                                   app_type_(app_type),
                                                   delay_sec_(delay_sec),
                                                   retry_(retry),
                                                   failed_reason_(failed_reason),
                                                   next_scheduled_time_(next_scheduled_time),
                                                   expiration_time_(expiration_time),
                                                   last_updated_time_(last_updated_time),
                                                   creation_time_(creation_time)
  {
  }

  void ToJson(Json::Value &json) const
  {
    json["id"] = id_;
    json["status"] = status_;
    json["ownerId"] = owner_id_;
    json["patientBirthDate"] = patient_birth_date_;
    json["patientId"] = patient_id_;
    json["patientName"] = patient_name_;
    json["patientSex"] = patient_sex_;
    json["accessionNumber"] = accession_number_;
    json["iuid"] = iuid_;
    json["resourceId"] = resource_id_;
    json["resourceType"] = resource_type_;
    json["app_id"] = app_id_;
    json["app_type"] = app_type_;
    json["delaySec"] = delay_sec_;
    json["retry"] = retry_;
    json["failedReason"] = failed_reason_;
    json["nextScheduledTime"] = next_scheduled_time_;
    json["expirationTime"] = expiration_time_;
    json["lastUpdatedTime"] = last_updated_time_;
    json["creationTime"] = creation_time_;
    json["now"] = boost::posix_time::to_iso_extended_string(Saola::GetNow());
  }

  std::string ToJsonString() const
  {
    std::stringstream ss;
    ss << "StableEventDTOGet {id=" << this->id_ << ", status=" << this->status_ << ", owner_id=" << this->owner_id_
       << ", patient_birth_date=" << this->patient_birth_date_ << ", patient_id=" << this->patient_id_ 
       << ", patient_name=" << this->patient_name_ << ", patient_sex=" << this->patient_sex_ 
       << ", accession_number=" << this->accession_number_
       << ", iuid=" << this->iuid_ << ", resource_id=" << this->resource_id_ << ", resource_type=" << this->resource_type_ 
       << ", app_id_=" << this->app_id_ << ", app_type_=" << this->app_type_ << ", delay_sec=" << delay_sec_ << ", retry_=" << this->retry_ 
       << ", failed_reason=" << this->failed_reason_ << ", next_scheduled_time=" << this->next_scheduled_time_ 
       << ", expiration_time=" << this->expiration_time_ << ", last_updated_time=" << this->last_updated_time_ << ", creation_time=" 
       << this->creation_time_ << ", now=" << boost::posix_time::to_iso_extended_string(Saola::GetNow()) 
       << ", elapsed=" << Saola::Elapsed(this->creation_time_) << "}";
    return ss.str();
  }
};
