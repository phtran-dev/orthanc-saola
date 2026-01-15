#pragma once

#include "../Constants.h"

#include <Toolbox.h>
#include <Logging.h>
#include <orthanc/OrthancCPlugin.h>

#include <string>
#include <map>

#include <json/value.h>

struct AppConfiguration
{
  static constexpr const char* Transfer = "Transfer";
  static constexpr const char* Exporter = "Exporter";
  static constexpr const char* StoreSCU = "StoreSCU";

  std::string id_;

  bool enable_ = true;

  std::string type_ = "";

  unsigned int delay_ = 0;

  std::string url_ = "";

  std::string authentication_ = "";

  OrthancPluginHttpMethod method_ = OrthancPluginHttpMethod_Post;

  int timeOut_ = 60;

  bool fieldMappingOverwrite = false;

  Json::Value fieldMapping_;

  Json::Value fieldValues_;

  std::string luaCallback_ = "";

  AppConfiguration()
  {
  }

  AppConfiguration(const Json::Value&appConfig)
  {
    this->id_ = appConfig["Id"].asString();
    this->enable_ = appConfig["Enable"].asBool();
    if (appConfig.isMember("Authentication"))
    {
      this->authentication_ = appConfig["Authentication"].asString();
    }
    if (appConfig.isMember("Method"))
    {
      std::string methodName = appConfig["Method"].asString();
      Orthanc::Toolbox::ToUpperCase(methodName);
      if (methodName == "POST")
      {
        this->method_ = OrthancPluginHttpMethod_Post;
      }
      else if (methodName == "GET")
      {
        this->method_ = OrthancPluginHttpMethod_Get;
      }
      else if (methodName == "PUT")
      {
        this->method_ = OrthancPluginHttpMethod_Put;
      }
      else if (methodName == "DELETE")
      {
        this->method_ = OrthancPluginHttpMethod_Delete;
      }
    }

    if (appConfig.isMember("LuaCallback") && !appConfig["LuaCallback"].isNull() && !appConfig["LuaCallback"].empty())
    {
      this->luaCallback_ = appConfig["LuaCallback"].asString();
    }

    this->type_ = appConfig["Type"].asString();
    this->url_ = appConfig["Url"].asString();

    if (appConfig.isMember("Delay"))
    {
      this->delay_ = appConfig["Delay"].asInt();
    }
    if (appConfig.isMember("Timeout"))
    {
      this->timeOut_ = appConfig["Timeout"].asInt();
    }

    if (this->type_ == "StoreServer" || this->type_ == "Ris")
    {
      // Default Mapping
      this->fieldMapping_["aeTitle"] =  RemoteAET;
      this->fieldMapping_["ipAddress"] = RemoteIP;
      this->fieldMapping_["accessionNumber"] = AccessionNumber;
      this->fieldMapping_["patientId"] = PatientID;
      this->fieldMapping_["patientName"] = PatientName;
      this->fieldMapping_["gender"] = PatientSex;
      this->fieldMapping_["patientSex"] = PatientSex;
      this->fieldMapping_["age"] = PatientAge;
      this->fieldMapping_["birthDate"] = PatientBirthDate;
      this->fieldMapping_["patientBirthDate"] = PatientBirthDate;
      this->fieldMapping_["bodyPartExamined"] = BodyPartExamined;
      this->fieldMapping_["description"] = StudyDescription;
      this->fieldMapping_["institutionName"] = InstitutionName;
      this->fieldMapping_["studyDate"] = StudyDate;
      this->fieldMapping_["studyTime"] = StudyTime;
      this->fieldMapping_["studyInstanceUID"] = StudyInstanceUID;
      this->fieldMapping_["manufacturerModelName"] = ManufacturerModelName;
      this->fieldMapping_["modalityType"] = Modality;
      this->fieldMapping_["numOfImages"] = CountInstances;
      this->fieldMapping_["numOfSeries"] = CountSeries;
      this->fieldMapping_["operatorName"] = OperatorsName;
      this->fieldMapping_["referringPhysician"] = ReferringPhysicianName;
      this->fieldMapping_["stationName"] = StationName;

      this->fieldMapping_["storeId"] = StoreID;
      this->fieldMapping_["storeNumOfStudies"] = CountStudies;
      this->fieldMapping_["storeSize"] = TotalDiskSizeMB;
      this->fieldMapping_["publicStudyUID"] = PublicStudyUID;
      this->fieldMapping_["studyNumOfSeries"] = CountSeries;
      this->fieldMapping_["studyNumOfInstances"] = CountInstances;
      this->fieldMapping_["studySize"] = DicomDiskSizeMB;
      this->fieldMapping_["modalitiesInStudy"] = ModalitiesInStudy;
      this->fieldMapping_["numberOfStudyRelatedSeries"] = NumberOfStudyRelatedSeries;
      this->fieldMapping_["numberOfStudyRelatedInstances"] = NumberOfStudyRelatedInstances;

      this->fieldMapping_["series"] = Series;
      this->fieldMapping_[std::string(Series) + "_seriesInstanceUID"] = Series_SeriesInstanceUID;
      this->fieldMapping_[std::string(Series) + "_seriesDate"] = Series_SeriesDate;
      this->fieldMapping_[std::string(Series) + "_seriesTime"] = Series_SeriesTime;
      this->fieldMapping_[std::string(Series) + "_modality"] = Series_Modality;
      this->fieldMapping_[std::string(Series) + "_manufacturer"] = Series_Manufacturer;
      this->fieldMapping_[std::string(Series) + "_stationName"] = Series_StationName;
      this->fieldMapping_[std::string(Series) + "_seriesDescription"] = Series_SeriesDescription;
      this->fieldMapping_[std::string(Series) + "_bodyPartExamined"] = Series_BodyPartExamined;
      this->fieldMapping_[std::string(Series) + "_sequenceName"] = Series_SequenceName;
      this->fieldMapping_[std::string(Series) + "_protocolName"] = Series_ProtocolName;
      this->fieldMapping_[std::string(Series) + "_seriesNumber"] = Series_SeriesNumber;
      this->fieldMapping_[std::string(Series) + "_cardiacNumberOfImages"] = Series_CardiacNumberOfImages;
      this->fieldMapping_[std::string(Series) + "_imagesInAcquisition"] = Series_ImagesInAcquisition;
      this->fieldMapping_[std::string(Series) + "_numberOfTemporalPositions"] = Series_NumberOfTemporalPositions;
      this->fieldMapping_[std::string(Series) + "_numOfImages"] = Series_NumOfImages;
      this->fieldMapping_[std::string(Series) + "_numOfSlices"] = Series_NumOfSlices;
      this->fieldMapping_[std::string(Series) + "_numOfTimeSlices"] = Series_NumOfTimeSlices;
      this->fieldMapping_[std::string(Series) + "_imageOrientationPatient"] = Series_ImageOrientationPatient;
      this->fieldMapping_[std::string(Series) + "_seriesType"] = Series_SeriesType;
      this->fieldMapping_[std::string(Series) + "_operatorsName"] = Series_OperatorsName;
      this->fieldMapping_[std::string(Series) + "_performedProcedureStepDescription"] = Series_PerformedProcedureStepDescription;
      this->fieldMapping_[std::string(Series) + "_acquisitionDeviceProcessingDescription"] = Series_AcquisitionDeviceProcessingDescription;
      this->fieldMapping_[std::string(Series) + "_contrastBolusAgent"] = Series_ContrastBolusAgent;
    }
    
    if (appConfig["FieldMappingOverwrite"].asBool())
    {
      this->fieldMapping_.clear();
    }

    if (appConfig.isMember("FieldMapping"))
    {
      if (appConfig["FieldMapping"].isArray())
      {
        for (auto &valueMap : appConfig["FieldMapping"])
        {
          for (Json::ValueConstIterator it = valueMap.begin(); it != valueMap.end(); ++it)
          {
            this->fieldMapping_[it.key().asString()] = *it;
          }
        }
      }
      else if (appConfig["FieldMapping"].isObject())
      {
        for (Json::ValueConstIterator it = appConfig["FieldMapping"].begin(); it != appConfig["FieldMapping"].end(); ++it)
        {
          this->fieldMapping_[it.key().asString()] = *it;
        }
      }
    }

    if (appConfig.isMember("FieldValues"))
    {
      if (appConfig["FieldValues"].isArray())
      {
        // This is for the case setting in json file as Orthanc Configuration is not able to read the Object type
        // Example
        // "FieldValues": [{"Peer": "LongTermPeer"}, {"Compression": "none"}] 
        this->fieldValues_.clear();
        for (auto &valueMap : appConfig["FieldValues"])
        {
          for (const auto &memberName : valueMap.getMemberNames())
          {
            this->fieldValues_[memberName] = valueMap[memberName.c_str()];
          }
        }
      }
      else if (appConfig["FieldValues"].isObject())
      {
        // This is for RestAPI response , for example from RQLITE
        // Example
        // "FieldValues": {"Peer": "LongTermPeer", "Compression": "none"}
        this->fieldValues_ = appConfig["FieldValues"];
      }
    }
  }

  void ToJson(Json::Value &json) const
  {
    json["Id"] = this->id_;
    json["Enable"] = this->enable_;
    json["Type"] = this->type_;
    json["Delay"] = this->delay_;
    json["Url"] = this->url_;
    json["Authentication"] = this->authentication_;
    json["Method"] = this->method_;
    json["Timeout"] = this->timeOut_;
    json["FieldMappingOverwrite"] = this->fieldMappingOverwrite;
    json["FieldMapping"] = this->fieldMapping_;
    json["FieldValues"] = this->fieldValues_;
    json["LuaCallback"] = this->luaCallback_;
  }
};