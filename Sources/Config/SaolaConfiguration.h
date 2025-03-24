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

  std::map<std::string, std::shared_ptr<AppConfiguration>> apps_;

  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  const std::shared_ptr<AppConfiguration> GetAppConfigurationById(const std::string& id) const;

  const std::map<std::string, std::shared_ptr<AppConfiguration>>& GetApps() const;

  void RemoveApp(const std::string& appId);

  bool IsEnabled() const;

  bool IsEnableRemoveFile() const;

  int GetThrottleExpirationDays() const;

  int GetMaxRetry() const;

  int GetThrottleDelayMs() const;
  
  const std::string& GetRoot() const;

  const std::string& GetDataBaseServerIdentifier() const;

  const std::string& GetDbPath() const;

  const bool EnableInMemJobCache() const;

  const int GetInMemJobCacheLimit() const;

  const std::string& GetInMemJobType() const;

  void ApplyConfiguration(const Json::Value& config, bool applyToDB = false);

  void UpdateConfiguration(const Json::Value& config);

  void ToJson(Json::Value& json);

};