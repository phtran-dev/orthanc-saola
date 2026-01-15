#pragma once

#include "AppConfiguration.h"
#include <memory>
#include <string>
#include <map>
#include <list>

#include "../Database/RQLite.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace Saola
{
class AppConfigRepository
{
private:
  bool isMemoryMode_ = true;

  boost::mutex mutex_;

  std::shared_ptr<rqlite::RqliteClient> rqliteClient_;

  std::map<std::string, std::shared_ptr<AppConfiguration>> inMemoryAppConfigs_;

  void OpenInMemory(const OrthancPlugins::OrthancConfiguration& saolaConfig);

  void OpenDB(const OrthancPlugins::OrthancConfiguration& saolaConfig);

  AppConfigRepository();

public:

  ~AppConfigRepository() = default;

  static AppConfigRepository& Instance();

  std::shared_ptr<AppConfiguration> Get(const std::string& id);

  void Get(const std::string& id, Json::Value& value);
  
  std::map<std::string, std::shared_ptr<AppConfiguration>> GetAll();

  void GetAll(Json::Value& value);

  void Save(const AppConfiguration& appConfig);

  void Delete(const std::string& id);

  void DeleteAll();
};
} // End of namespace Saola
