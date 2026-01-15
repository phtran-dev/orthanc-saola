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
  
  int defaultJobLockDuration_ = 3600;
  
  std::map<std::string, int> jobLockDurations_;

  int maxRetry_ = 5;

  int queryLimit_ = 10;

  int throttleDelayMs_ = 100;

  std::string root_;

  std::string databaseServerIdentifier_;

  std::string dbPath_;

  int pollingDBIntervalInSeconds_ = 30; // 30 seconds

  std::string dataSourceDriver_ = "org.sqlite.Driver";
  std::string dataSourceUrl_ = "";

  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  void ToJson(Json::Value& json);

  int GetJobLockDuration(const std::string& appType) const
  {
    auto it = jobLockDurations_.find(appType);
    if (it != jobLockDurations_.end())
    {
      return it->second;
    }
    return defaultJobLockDuration_;
  }
  
  const std::map<std::string, int>& GetJobLockDurations() const
  {
      return jobLockDurations_;
  }
  
  int GetDefaultJobLockDuration() const 
  {
      return defaultJobLockDuration_;
  }

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