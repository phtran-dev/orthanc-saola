#pragma once

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"


#include <OrthancFramework.h>  // To have ORTHANC_ENABLE_SQLITE defined
#include <SQLite/Connection.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>

#include <json/value.h>

namespace Itech
{
  class AppConfigDatabase : public boost::noncopyable
  {
  private:
    boost::mutex                 mutex_;
    OrthancPlugins::HttpClient   client_;
    std::string                  url_;
    void Initialize();

  public:
    static AppConfigDatabase& Instance();
    void Open(const std::string& url);
    void OpenInMemory();  // For unit tests

    void GetAppConfigs(Json::Value& appConfigs);
    void GetAppConfigById(Json::Value& appConfig, const std::string& id);
    bool DeleteAppConfigById(const std::string& id);
    void SaveAppConfig(Json::Value& appConfig);
  };
}