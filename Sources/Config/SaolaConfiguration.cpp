#include "SaolaConfiguration.h"
#include "AppConfigRepository.h"

#include "../Constants.h"
#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

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
  this->queryLimit_ = saola.GetIntegerValue("QueryLimit", 10); 

  this->databaseServerIdentifier_ = OrthancPluginGetDatabaseServerIdentifier(OrthancPlugins::GetGlobalContext());
  std::string pathStorage = configuration.GetStringValue(STORAGE_DIRECTORY, ORTHANC_STORAGE);
  LOG(WARNING) << "SaolaConfiguration - Path to the storage area: " << pathStorage;
  this->dataSourceDriver_ = saola.GetStringValue("DataSource.Driver", "org.sqlite.Driver");
  this->dataSourceUrl_ = saola.GetStringValue("DataSource.Url", "");
  
  // Determine driver type for factory
  if (this->dataSourceDriver_ == "io.rqlite.Driver")
  {
     // RQLite
     if (!this->dataSourceUrl_.empty() && !boost::starts_with(this->dataSourceUrl_, "jdbc:rqlite:"))
     {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, 
              "SaolaConfiguration: io.rqlite.Driver requires DataSource.Url to start with 'jdbc:rqlite:'");
     }
  }
  else
  {
    // SQLite
    if (!this->dataSourceUrl_.empty())
    {
       if (boost::starts_with(this->dataSourceUrl_, "jdbc:") && !boost::starts_with(this->dataSourceUrl_, "jdbc:sqlite:"))
       {
           throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, 
              "SaolaConfiguration: org.sqlite.Driver requires DataSource.Url to start with 'jdbc:sqlite:' or be a file path");
       }
    }

    // If DataSource.Url is empty, fallback to old logic for default dbPath
    if (this->dataSourceUrl_.empty())
    {
       boost::filesystem::path defaultDbPath = boost::filesystem::path(pathStorage) / (DB_NAME + "." + databaseServerIdentifier_ + ".db");
       std::string path = saola.GetStringValue("Path", defaultDbPath.string());
       // Construct a DataSourceUrl from this for consistency
       this->dataSourceUrl_ = "jdbc:sqlite:" + path;
    }
  }

  this->enableInMemJobCache_ = saola.GetBooleanValue("EnableInMemJobCache", false);
  this->inMemJobCacheLimit_ = saola.GetIntegerValue("InMemJobCacheLimit", 100);
  // ["DicomModalityStore", "Transfer"];
  saola.LookupSetOfStrings(this->inMemJobTypes_, "InMemJobCacheTypes", true);
  if (this->enableInMemJobCache_ && this->inMemJobTypes_.empty())
  {
    // Set Default
    this->inMemJobTypes_.insert("DicomModalityStore");
    this->inMemJobTypes_.insert("PushTransfer");
    this->inMemJobTypes_.insert("Exporter");
  }
  this->pollingDBIntervalInSeconds_ = saola.GetIntegerValue("PollingDBInSeconds", 30);
}

SaolaConfiguration &SaolaConfiguration::Instance()
{
  static SaolaConfiguration configuration_;
  return configuration_;
}


void SaolaConfiguration::ToJson(Json::Value &json)
{
  json["Enable"] = this->enable_;
  json["EnableRemoveFile"] = this->enableRemoveFile_;
  json["EnableInMemJobCache"] = this->enableInMemJobCache_;
  json["InMemJobCacheLimit"] = this->inMemJobCacheLimit_;
  json["InMemJobTypes"] = Json::arrayValue;
  for (const auto& inMemJobType : this->inMemJobTypes_)
  {
    json["InMemJobTypes"].append(inMemJobType);
  }
  json["ThrottleExpirationDays"] = this->throttleExpirationDays_;
  json["MaxRetry"] = this->maxRetry_;
  json["ThrottleDelayMs"] = this->throttleDelayMs_;
  json["Root"] = this->root_;
  json["DatabaseServerIdentifier"] = this->databaseServerIdentifier_;
  json["DbPath"] = ""; // Deprecated
  json["PollingDBIntervalInSeconds"] = this->pollingDBIntervalInSeconds_;
  json["PollingDBIntervalInSeconds"] = this->pollingDBIntervalInSeconds_;
  json["DataSource.Driver"] = this->dataSourceDriver_;
  json["DataSource.Url"] = this->dataSourceUrl_;

  json["Apps"] = Json::arrayValue;
  auto apps = Saola::AppConfigRepository::Instance().GetAll();
  for (const auto& app : apps)
  {
    Json::Value appJson;
    app.second->ToJson(appJson);
    json["Apps"].append(appJson);
  }
}