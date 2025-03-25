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

  std::string inMemJobType_;

  int throttleExpirationDays_;

  int maxRetry_ = 5;

  int throttleDelayMs_;

  std::string root_;

  std::string databaseServerIdentifier_;

  std::string dbPath_;

  int pollingDBIntervalInSeconds_ = 30; // 30 seconds

  std::map<std::string, std::shared_ptr<AppConfiguration>> apps_;

  std::string appConfigDataSourceUrl_ = "";

  int appConfigDataSourceTimeout_ = 1; // In second(s)

  int appConfigDataSourcePollingInterval_ = 30; // In second(s)

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

  const std::string& GetDbPath() const
  {
    return this->dbPath_;
  }

  const bool EnableInMemJobCache() const
  {
    return this->enableInMemJobCache_;
  }
  
  const int GetInMemJobCacheLimit() const
  {
    return this->inMemJobCacheLimit_;
  }

  const std::string& GetInMemJobType() const
  {
    return this->inMemJobType_;
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

};