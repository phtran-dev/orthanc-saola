#pragma once

#include "RQLite.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"


#include <OrthancFramework.h>  // To have ORTHANC_ENABLE_SQLITE defined
#include <SQLite/Connection.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>

#include <json/value.h>

namespace Saola
{
  class AppConfigDatabase : public boost::noncopyable
  {
  private:
    boost::mutex                 mutex_;
    OrthancPlugins::HttpClient   client_;
    std::unique_ptr<rqlite::RqliteClient>         rqliteClient_;
    std::string                  url_;
    int                          timeout_ = 1;
    bool                         enabled_;
    void Initialize();

  public:
    static AppConfigDatabase& Instance();
    void Open(const std::string& url, int timeout);
    void GetAppConfigs(Json::Value& appConfigs);
    void GetAppConfigById(Json::Value& appConfig, const std::string& id);
    bool DeleteAppConfigById(const std::string& id);
    void SaveAppConfig(const Json::Value& appConfig);
    bool IsEnabled() const;
  };
}