#pragma once

#include "AppConfiguration.h"

#include <list>
#include <string>
#include <memory> 

class SaolaConfiguration
{
private:

  bool enable_;

  bool enableRemoveFile_;

  bool enableInMemJobCache_;

  int inMemJobCacheLimit_;

  std::set<std::string> inMemJobTypes_;

  int throttleExpirationDays_;

  int maxRetry_ = 5;

  int queryLimit_ = 10;

  int throttleDelayMs_ = 100;

  std::string root_;

  std::string databaseServerIdentifier_;

  std::string dbPath_;

  int pollingDBIntervalInSeconds_ = 30; // 30 seconds

  std::map<std::string, std::shared_ptr<AppConfiguration>> apps_;

  std::string appConfigDataSourceUrl_ = "";

  int appConfigDataSourceTimeout_ = 1; // In second(s)

  int appConfigDataSourcePollingInterval_ = 30; // In second(s)

  std::string dataSourceDriver_ = "org.sqlite.Driver";
  std::string dataSourceUrl_ = "";

  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  const std::shared_ptr<AppConfiguration> GetAppConfigurationById(const std::string& id) const;

  void GetApps(std::map<std::string, std::shared_ptr<AppConfiguration>>&) const;

  void ApplyConfigurations(const Json::Value& appConfigs, bool clear);

  void UpdateConfiguration(const Json::Value& config);

  void ToJson(Json::Value& json);

  void EraseApps()
  {
    this->apps_.clear();
  }

  void RemoveApp(const std::string& appId);

  bool IsEnabled() const
  {
    return this->enable_;
  }
  
  bool IsEnableRemoveFile() const
  {
    return this->enableRemoveFile_;
  }

  int GetThrottleExpirationDays() const
  {
    return this->throttleExpirationDays_;
  }

  int GetMaxRetry() const
  {
    return this->maxRetry_;
  }

  int GetQueryLimit() const
  {
    return this->queryLimit_;
  }
  

  int GetThrottleDelayMs() const
  {
    return this->throttleDelayMs_;
  }

  const std::string& GetRoot() const
  {
    return this->root_;
  }

  const std::string& GetDataBaseServerIdentifier() const
  {
    return this->databaseServerIdentifier_;
  }


  const bool EnableInMemJobCache() const
  {
    return this->enableInMemJobCache_;
  }
  
  const int GetInMemJobCacheLimit() const
  {
    return this->inMemJobCacheLimit_;
  }

  const std::set<std::string>& GetInMemJobTypes() const
  {
    return this->inMemJobTypes_;
  }

  const std::string& GetAppConfigDataSourceUrl() const
  {
    return this->appConfigDataSourceUrl_;
  }

  int GetAppConfigDataSourceTimeout() const
  {
    return this->appConfigDataSourceTimeout_;
  }

  int GetppConfigDataSourcePollingInterval() const
  {
    return this->appConfigDataSourcePollingInterval_;
  }

  const std::string& GetDataSourceDriver() const
  {
    return this->dataSourceDriver_;
  }

  const std::string& GetDataSourceUrl() const
  {
    return this->dataSourceUrl_;
  }

  std::string GetDatabaseDriver() const
  {
    return (dataSourceDriver_ == "io.rqlite.Driver") ? "rqlite" : "sqlite";
  }



};