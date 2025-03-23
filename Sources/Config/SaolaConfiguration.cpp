#include "SaolaConfiguration.h"
#include "../Constants.h"
#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include "../Database/AppConfigDatabase.h"

#include <EmbeddedResources.h>

#include <Toolbox.h>
#include <Logging.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

static const std::string ORTHANC_STORAGE = "OrthancStorage";
static const std::string STORAGE_DIRECTORY = "StorageDirectory";
static std::string DB_NAME = "saola-plugin";

SaolaConfiguration::SaolaConfiguration(/* args */)
{
  OrthancPlugins::OrthancConfiguration configuration;
  OrthancPlugins::OrthancConfiguration saola;
  configuration.GetSection(saola, "Saola");

  this->enable_ = saola.GetBooleanValue("Enable", false);
  this->enableRemoveFile_ = saola.GetBooleanValue("EnableRemoveFile", false);
  this->throttleExpirationDays_ = saola.GetIntegerValue("ThrottleExpirationDays", 2);
  this->root_ = saola.GetStringValue("Root", "/saola/");
  this->maxRetry_ = saola.GetIntegerValue("MaxRetry", 5);
  this->throttleDelayMs_ = saola.GetIntegerValue("ThrottleDelayMs", 100); // Default 100 milliseconds

  this->databaseServerIdentifier_ = OrthancPluginGetDatabaseServerIdentifier(OrthancPlugins::GetGlobalContext());
  std::string pathStorage = configuration.GetStringValue(STORAGE_DIRECTORY, ORTHANC_STORAGE);
  LOG(WARNING) << "SaolaConfiguration - Path to the storage area: " << pathStorage;
  boost::filesystem::path defaultDbPath = boost::filesystem::path(pathStorage) / (DB_NAME + "." + databaseServerIdentifier_ + ".db");
  this->dbPath_ = saola.GetStringValue("Path", defaultDbPath.string());

  this->enableInMemJobCache_ = saola.GetBooleanValue("EnableInMemJobCache", false);
  this->inMemJobCacheLimit_ = saola.GetIntegerValue("InMemJobCacheLimit", 100);
  this->inMemJobType_ = saola.GetStringValue("InMemJobCacheType", "DicomModalityStore");


  if (!saola.GetStringValue("DataSource.Url", "").empty())
  {
    Saola::AppConfigDatabase::Instance().Open(saola.GetStringValue("DataSource.Url", ""));
    Json::Value apps;
    Saola::AppConfigDatabase::Instance().GetAppConfigs(apps);
    this->ApplyConfiguration(apps);
  }
  else if (saola.GetJson().isMember("Apps"))
  {
    this->ApplyConfiguration(saola.GetJson()["Apps"]);
  }
}

SaolaConfiguration &SaolaConfiguration::Instance()
{
  static SaolaConfiguration configuration_;
  return configuration_;
}


const std::shared_ptr<AppConfiguration> SaolaConfiguration::GetAppConfigurationById(const std::string &id) const
{
  const auto appIt = this->apps_.find(id);
  if (appIt != this->apps_.end())
  {
    return appIt->second;
  }

  return std::shared_ptr<AppConfiguration>();
}

const std::map<std::string, std::shared_ptr<AppConfiguration>> &SaolaConfiguration::GetApps() const
{
  return this->apps_;
}

int SaolaConfiguration::GetMaxRetry() const
{
  return this->maxRetry_;
}

int SaolaConfiguration::GetThrottleDelayMs() const
{
  return this->throttleDelayMs_;
}

bool SaolaConfiguration::IsEnabled() const
{
  return this->enable_;
}

bool SaolaConfiguration::IsEnableRemoveFile() const
{
  return this->enableRemoveFile_;
}

int SaolaConfiguration::GetThrottleExpirationDays() const
{
  return this->throttleExpirationDays_;
}

const std::string &SaolaConfiguration::GetRoot() const
{
  return this->root_;
}

const std::string &SaolaConfiguration::GetDataBaseServerIdentifier() const
{
  return this->databaseServerIdentifier_;
}

const std::string &SaolaConfiguration::GetDbPath() const
{
  return this->dbPath_;
}

const bool SaolaConfiguration::EnableInMemJobCache() const
{
  return this->enableInMemJobCache_;
}

const int SaolaConfiguration::GetInMemJobCacheLimit() const
{
  return this->inMemJobCacheLimit_;
}

const std::string &SaolaConfiguration::GetInMemJobType() const
{
  return this->inMemJobType_;
}

void SaolaConfiguration::ApplyConfiguration(const Json::Value &appConfigs, bool applyToDB)
{
  std::list<std::shared_ptr<AppConfiguration>> apps;
  for (const auto &appConfig : appConfigs)
  {
    // Validate configurations
    if (!appConfig.isMember("Id") || !appConfig.isMember("Type") || !appConfig.isMember("Url") || !appConfig.isMember("Enable"))
    {
      LOG(ERROR) << "[SaolaConfiguration] ERROR Missing mandatory configurations: Id, Type, Url, Enable";
      continue;
    }

    if (!appConfig["Enable"].asBool())
    {
      continue;
    }

    if (this->apps_.find(appConfig["Id"].asString()) != this->apps_.end())
    {
      continue;
    }

    std::shared_ptr<AppConfiguration> app = std::make_shared<AppConfiguration>();
    app->id_ = appConfig["Id"].asString();
    app->enable_ = appConfig["Enable"].asBool();
    if (appConfig.isMember("Authentication"))
    {
      app->authentication_ = appConfig["Authentication"].asString();
    }
    if (appConfig.isMember("Method"))
    {
      std::string methodName = appConfig["Method"].asString();
      Orthanc::Toolbox::ToUpperCase(methodName);
      if (methodName == "POST")
      {
        app->method_ = OrthancPluginHttpMethod_Post;
      }
      else if (methodName == "GET")
      {
        app->method_ = OrthancPluginHttpMethod_Get;
      }
      else if (methodName == "PUT")
      {
        app->method_ = OrthancPluginHttpMethod_Put;
      }
      else if (methodName == "DELETE")
      {
        app->method_ = OrthancPluginHttpMethod_Delete;
      }
    }

    if (appConfig.isMember("LuaCallback") && !appConfig["LuaCallback"].isNull() && !appConfig["LuaCallback"].empty())
    {
      app->luaCallback_ = appConfig["LuaCallback"].asString();
    }

    app->type_ = appConfig["Type"].asString();
    app->url_ = appConfig["Url"].asString();

    if (appConfig.isMember("Delay"))
    {
      app->delay_ = appConfig["Delay"].asInt();
    }
    if (appConfig.isMember("Timeout"))
    {
      app->timeOut_ = appConfig["Timeout"].asInt();
    }

    if (app->type_ == "StoreServer" || app->type_ == "Ris")
    {
      // Default Mapping
      app->fieldMapping_["aeTitle"] =  "RemoteAET";
      app->fieldMapping_["ipAddress"] = "RemoteIP";
      app->fieldMapping_["accessionNumber"] = "AccessionNumber";
      app->fieldMapping_["patientId"] = "PatientID";
      app->fieldMapping_["patientName"] = "PatientName";
      app->fieldMapping_["gender"] = "PatientSex";
      app->fieldMapping_["age"] = "PatientAge";
      app->fieldMapping_["birthDate"] = "PatientBirthDate";
      app->fieldMapping_["bodyPartExamined"] = "BodyPartExamined";
      app->fieldMapping_["description"] = "StudyDescription";
      app->fieldMapping_["institutionName"] = "InstitutionName";
      app->fieldMapping_["studyDate"] = "StudyDate";
      app->fieldMapping_["studyTime"] = "StudyTime";
      app->fieldMapping_["studyInstanceUID"] = "StudyInstanceUID";
      app->fieldMapping_["manufacturerModelName"] = "ManufacturerModelName";
      app->fieldMapping_["modalityType"] = "Modality";
      app->fieldMapping_["numOfImages"] = "CountInstances";
      app->fieldMapping_["numOfSeries"] = "CountSeries";
      app->fieldMapping_["operatorName"] = "OperatorsName";
      app->fieldMapping_["referringPhysician"] = "ReferringPhysicianName";
      app->fieldMapping_["stationName"] = "StationName";

      app->fieldMapping_["storeId"] = "StoreID";
      app->fieldMapping_["storeNumOfStudies"] = "CountStudies";
      app->fieldMapping_["storeSize"] = "TotalDiskSizeMB";
      app->fieldMapping_["publicStudyUID"] = "PublicStudyUID";
      app->fieldMapping_["studyNumOfSeries"] = "CountSeries";
      app->fieldMapping_["studyNumOfInstances"] = "CountInstances";
      app->fieldMapping_["studySize"] = "DicomDiskSizeMB";
      app->fieldMapping_["modalitiesInStudy"] = "ModalitiesInStudy";
      app->fieldMapping_["numberOfStudyRelatedSeries"] = "NumberOfStudyRelatedSeries";
      app->fieldMapping_["numberOfStudyRelatedInstances"] = "NumberOfStudyRelatedInstances";

      app->fieldMapping_["series"] = Series;
      app->fieldMapping_[std::string(Series) + "_seriesInstanceUID"] = Series_SeriesInstanceUID;
      app->fieldMapping_[std::string(Series) + "_seriesDate"] = Series_SeriesDate;
      app->fieldMapping_[std::string(Series) + "_seriesTime"] = Series_SeriesTime;
      app->fieldMapping_[std::string(Series) + "_modality"] = Series_Modality;
      app->fieldMapping_[std::string(Series) + "_manufacturer"] = Series_Manufacturer;
      app->fieldMapping_[std::string(Series) + "_stationName"] = Series_StationName;
      app->fieldMapping_[std::string(Series) + "_seriesDescription"] = Series_SeriesDescription;
      app->fieldMapping_[std::string(Series) + "_bodyPartExamined"] = Series_BodyPartExamined;
      app->fieldMapping_[std::string(Series) + "_sequenceName"] = Series_SequenceName;
      app->fieldMapping_[std::string(Series) + "_protocolName"] = Series_ProtocolName;
      app->fieldMapping_[std::string(Series) + "_seriesNumber"] = Series_SeriesNumber;
      app->fieldMapping_[std::string(Series) + "_cardiacNumberOfImages"] = Series_CardiacNumberOfImages;
      app->fieldMapping_[std::string(Series) + "_imagesInAcquisition"] = Series_ImagesInAcquisition;
      app->fieldMapping_[std::string(Series) + "_numberOfTemporalPositions"] = Series_NumberOfTemporalPositions;
      app->fieldMapping_[std::string(Series) + "_numOfImages"] = Series_NumOfImages;
      app->fieldMapping_[std::string(Series) + "_numOfSlices"] = Series_NumOfSlices;
      app->fieldMapping_[std::string(Series) + "_numOfTimeSlices"] = Series_NumOfTimeSlices;
      app->fieldMapping_[std::string(Series) + "_imageOrientationPatient"] = Series_ImageOrientationPatient;
      app->fieldMapping_[std::string(Series) + "_seriesType"] = Series_SeriesType;
      app->fieldMapping_[std::string(Series) + "_operatorsName"] = Series_OperatorsName;
      app->fieldMapping_[std::string(Series) + "_performedProcedureStepDescription"] = Series_PerformedProcedureStepDescription;
      app->fieldMapping_[std::string(Series) + "_acquisitionDeviceProcessingDescription"] = Series_AcquisitionDeviceProcessingDescription;
      app->fieldMapping_[std::string(Series) + "_contrastBolusAgent"] = Series_ContrastBolusAgent;
    }

    if (appConfig["FieldMappingOverwrite"].asBool())
    {
      app->fieldMapping_.clear();
    }

    for (auto &valueMap : appConfig["FieldMapping"])
    {
      for (Json::ValueConstIterator it = valueMap.begin(); it != valueMap.end(); ++it)
      {
        app->fieldMapping_[it.key().asString()] = *it;
      }
    }

    for (auto &valueMap : appConfig["FieldValues"])
    {
      for (const auto &memberName : valueMap.getMemberNames())
      {
        app->fieldValues_[memberName] = valueMap[memberName.c_str()];
      }
    }

    this->apps_.emplace(app->id_, app);
    if (applyToDB)
    {
      // Save to DB
      Saola::AppConfigDatabase::Instance().SaveAppConfig(appConfig);
    }
  }
}

void SaolaConfiguration::ToJson(Json::Value &json)
{
  json["enable_"] = this->enable_;
  json["root_"] = this->root_;
  json["maxRetry_"] = this->maxRetry_;
  json["apps_"] = Json::arrayValue;

  for (const auto &app : this->apps_)
  {
    Json::Value appJson;
    app.second->ToJson(appJson);
    json["apps_"].append(appJson);
  }
}