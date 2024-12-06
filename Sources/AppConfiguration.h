#pragma once

#include <string>
#include <map> 

struct AppConfiguration
{

  bool enable;

  std::string type;

  unsigned int delay = 0;

  std::string url;

  std::string authentication;

  std::map<std::string, std::string> fieldMapping_;

  std::map<std::string, std::string> fieldValues_;

  AppConfiguration()
  {
  }

};