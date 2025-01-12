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

  int throttleExpirationDays_;

  int maxRetry_ = 5;

  int throttleDelayMs_;

  std::string root_;

  std::list<std::shared_ptr<AppConfiguration>> apps_;
  
  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  const std::shared_ptr<AppConfiguration> GetAppConfigurationById(const std::string& id) const;

  const std::list<std::shared_ptr<AppConfiguration>>& GetApps() const;

  bool IsEnabled() const;

  bool IsEnableRemoveFile() const;

  int GetThrottleExpirationDays() const;

  int GetMaxRetry() const;

  int GetThrottleDelayMs() const;
  
  const std::string& GetRoot() const;

  void ToJson(Json::Value& json);

};