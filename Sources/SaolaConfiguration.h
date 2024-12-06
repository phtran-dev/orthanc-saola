#pragma once


#include "AppConfiguration.h"

#include <list>
#include <string>
#include <memory> 



class SaolaConfiguration
{
private:

  bool enable_;

  std::string root_;

  std::list<std::shared_ptr<AppConfiguration>> apps_;
  
  SaolaConfiguration(/* args */);

public:

  static SaolaConfiguration& Instance();

  void GetAppConfiguration(const std::string& app, std::list<std::shared_ptr<AppConfiguration>>& res);

  bool IsEnabled() const;
  
  const std::string& GetRoot() const;

};