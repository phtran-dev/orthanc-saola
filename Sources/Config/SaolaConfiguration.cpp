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
  this->queryLimit_ = saola.GetIntegerValue("QueryLimit", 100); 

  this->databaseServerIdentifier_ = OrthancPluginGetDatabaseServerIdentifier(OrthancPlugins::GetGlobalContext());
  std::string pathStorage = configuration.GetStringValue(STORAGE_DIRECTORY, ORTHANC_STORAGE);
  LOG(WARNING) << "SaolaConfiguration - Path to the storage area: " << pathStorage;
  boost::filesystem::path defaultDbPath = boost::filesystem::path(pathStorage) / (DB_NAME + "." + databaseServerIdentifier_ + ".db");
  this->dbPath_ = saola.GetStringValue("Path", defaultDbPath.string());

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

  this->appConfigDataSourceUrl_ = saola.GetStringValue("AppConfig.DataSource.Url", "");
  this->appConfigDataSourceTimeout_ = saola.GetIntegerValue("AppConfig.DataSource.Timeout", 1);
  this->appConfigDataSourcePollingInterval_ = saola.GetIntegerValue("AppConfig.DataSource.PollingInterval", 30);

  if (!this->appConfigDataSourceUrl_.empty())
  {
    Saola::AppConfigDatabase::Instance().Open(this->appConfigDataSourceUrl_, this->appConfigDataSourceTimeout_);
    Json::Value apps;
    Saola::AppConfigDatabase::Instance().GetAppConfigs(apps);
    this->ApplyConfigurations(apps, true);
  }
  else if (saola.GetJson().isMember("Apps"))
  {
    this->ApplyConfigurations(saola.GetJson()["Apps"], true);
  }
}

SaolaConfiguration &SaolaConfiguration::Instance()
{
  static SaolaConfiguration configuration_;
  return configuration_;
}


const std::shared_ptr<AppConfiguration> SaolaConfiguration::GetAppConfigurationById(const std::string &id) const
{
  if (Saola::AppConfigDatabase::Instance().IsEnabled())
  {
    Json::Value appConfig;
    Saola::AppConfigDatabase::Instance().GetAppConfigById(appConfig, id);
    if (!appConfig.empty())
    {
      return std::make_shared<AppConfiguration>(appConfig);
    }
  }

  const auto appIt = this->apps_.find(id);
  if (appIt != this->apps_.end())
  {
    return appIt->second;
  }

  return std::shared_ptr<AppConfiguration>();
}

void SaolaConfiguration::GetApps(std::map<std::string, std::shared_ptr<AppConfiguration>> &configMap) const
{
  if (Saola::AppConfigDatabase::Instance().IsEnabled())
  {
    Json::Value appConfigs;
    Saola::AppConfigDatabase::Instance().GetAppConfigs(appConfigs);
    if (!appConfigs.empty())
    {
      std::map<std::string, std::shared_ptr<AppConfiguration>> configMap;
      for (const auto& appConfig : appConfigs)
      {
        configMap.emplace(appConfig["Id"].asString(), std::make_shared<AppConfiguration>(appConfig));
      }
      return;
    }
  }

  for (const auto& app : this->apps_)
  {
    configMap.emplace(app.first, app.second);
  }
}

void SaolaConfiguration::RemoveApp(const std::string& appId)
{
  auto it = this->apps_.find(appId);
  if (it != this->apps_.end())
  {
    this->apps_.erase(it);
  }
}

void SaolaConfiguration::ApplyConfigurations(const Json::Value &appConfigs, bool clear)
{
  if (clear)
  {
    this->apps_.clear();
  }
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

    Saola::AppConfigDatabase::Instance().SaveAppConfig(appConfig);

    std::string id = appConfig["Id"].asString();
    auto appIT = this->apps_.find(id);
    if (appIT != this->apps_.end())
    {
      this->apps_.erase(appIT);
    }

    this->apps_.emplace(id, std::make_shared<AppConfiguration>(appConfig));
  }
}

void SaolaConfiguration::UpdateConfiguration(const Json::Value &appConfig)
{
  auto appIT = this->apps_.find(appConfig["Id"].asString());
  if (appIT == this->apps_.end())
  {
    return;
  }

  if (appConfig.isMember("Enable"))
  {
    appIT->second->enable_ = appConfig["Enable"].asBool();
  }

  if (appConfig.isMember("Authentication"))
  {
    appIT->second->authentication_ = appConfig["Authentication"].asString();
  }

  if (appConfig.isMember("Method"))
  {
    std::string methodName = appConfig["Method"].asString();
    Orthanc::Toolbox::ToUpperCase(methodName);
    if (methodName == "POST")
    {
      appIT->second->method_ = OrthancPluginHttpMethod_Post;
    }
    else if (methodName == "GET")
    {
      appIT->second->method_ = OrthancPluginHttpMethod_Get;
    }
    else if (methodName == "PUT")
    {
      appIT->second->method_ = OrthancPluginHttpMethod_Put;
    }
    else if (methodName == "DELETE")
    {
      appIT->second->method_ = OrthancPluginHttpMethod_Delete;
    }
  }

  if (appConfig.isMember("LuaCallback"))
  {
    appIT->second->luaCallback_ = appConfig["LuaCallback"].asString();
  }

  if (appConfig.isMember("Type"))
  {
    appIT->second->type_ = appConfig["Type"].asString();
  }

  if (appConfig.isMember("Url"))
  {
    appIT->second->url_ = appConfig["Url"].asString();
  }

  if (appConfig.isMember("Delay"))
  {
    appIT->second->delay_ = appConfig["Delay"].asInt();
  }
  
  if (appConfig.isMember("Timeout"))
  {
    appIT->second->timeOut_ = appConfig["Timeout"].asInt();
  }

  if (appConfig["FieldMappingOverwrite"].asBool())
  {
    appIT->second->fieldMapping_.clear();
  }

  for (auto &valueMap : appConfig["FieldMapping"])
  {
    for (Json::ValueConstIterator it = valueMap.begin(); it != valueMap.end(); ++it)
    {
      appIT->second->fieldMapping_[it.key().asString()] = *it;
    }
  }

  for (auto &valueMap : appConfig["FieldValues"])
  {
    for (const auto &memberName : valueMap.getMemberNames())
    {
      appIT->second->fieldValues_[memberName] = valueMap[memberName.c_str()];
    }
  }
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
  json["DbPath"] = this->dbPath_;
  json["PollingDBIntervalInSeconds"] = this->pollingDBIntervalInSeconds_;

  json["Apps"] = Json::arrayValue;

  if (Saola::AppConfigDatabase::Instance().IsEnabled())
  {
    Json::Value appConfigs;
    Saola::AppConfigDatabase::Instance().GetAppConfigs(appConfigs);
    if (!appConfigs.empty())
    {
      this->apps_.clear();
      for (const auto& appConfig: appConfigs)
      {
        std::shared_ptr<AppConfiguration> config(new AppConfiguration(appConfig));
        this->apps_.emplace(config->id_, config);
      }
    }
  }

  for (const auto &app : this->apps_)
  {
    Json::Value appJson;
    app.second->ToJson(appJson);
    json["Apps"].append(appJson);
  }
}