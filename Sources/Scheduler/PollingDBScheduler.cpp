#include "PollingDBScheduler.h"

#include "../Database/AppConfigDatabase.h"

#include "../SaolaDatabase.h"
#include "../TimeUtil.h"
#include "../Config/SaolaConfiguration.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

PollingDBScheduler &PollingDBScheduler::Instance()
{
  static PollingDBScheduler instance;
  return instance;
}

PollingDBScheduler::~PollingDBScheduler()
{
  if (this->m_state == State_Running)
  {
    OrthancPlugins::LogError("PollingDBScheduler::Stop() should have been manually called");
    Stop();
  }
}


void PollingDBScheduler::MonitorDatabase()
{
  while (this->m_state == State_Running)
  {
    Json::Value appConfigs;
    Saola::AppConfigDatabase::Instance().GetAppConfigs(appConfigs);

    std::map<std::string, Json::Value> appConfigMap;
    for (const auto& appConfig : appConfigs)
    {
      appConfigMap[appConfig["Id"].asString()] = appConfig;
    }

    Json::Value newAppConfigs = Json::arrayValue;
    for (const auto& appConfig : appConfigMap)
    {
      if (SaolaConfiguration::Instance().GetApps().find(appConfig.first) == SaolaConfiguration::Instance().GetApps().end())
      {
        newAppConfigs.append(appConfig.second);
      }
      else
      {
        SaolaConfiguration::Instance().UpdateConfiguration(appConfig.second);
      }
    }

    if (!newAppConfigs.empty())
    {
      // Add new AppConfiguration
      SaolaConfiguration::Instance().ApplyConfiguration(newAppConfigs);
    }

    for (const auto& app : SaolaConfiguration::Instance().GetApps())
    {
      if (appConfigMap.find(app.first) == appConfigMap.end())
      {
        // Remove AppConfiguration
        SaolaConfiguration::Instance().RemoveApp(app.first);
      }
    }

    unsigned int intervalSeconds = 20;
    for (unsigned int i = 0; i < intervalSeconds * 10; i++)
    {
      if (this->m_state != State_Running)
      {
        return;
      }

      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
}

void PollingDBScheduler::Start()
{
  if (!Saola::AppConfigDatabase::Instance().IsEnabled())
  {
    return;
  }
  if (this->m_state != State_Setup)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
  }

  this->m_state = State_Running;
  this->m_worker = new std::thread([this]() {
    while (this->m_state == State_Running)
    {
      this->MonitorDatabase();
    } });
}

void PollingDBScheduler::Stop()
{
  if (this->m_state == State_Running)
  {
    this->m_state = State_Done;
    if (this->m_worker->joinable())
      this->m_worker->join();
    delete this->m_worker;
  }
}
