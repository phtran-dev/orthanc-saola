#pragma once


#include "AppConfiguration.h"

#include <list>
#include <string>
#include <memory> 


class SaolaConfiguration
{
private:

  bool enable_;

  int maxRetry_ = 5;

  std::string root_;

  std::list<std::shared_ptr<AppConfiguration>> apps_;
  
  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  bool GetAppConfigurationById(const std::string& id, AppConfiguration& res);

  const std::list<std::shared_ptr<AppConfiguration>>& GetApps() const;

  bool IsEnabled() const;

  int GetMaxRetry() const;
  
  const std::string& GetRoot() const;

  void ToJson(Json::Value& json);

};