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
    if (Saola::AppConfigDatabase::Instance().IsEnabled())
    {
      Json::Value appConfigs;
      Saola::AppConfigDatabase::Instance().GetAppConfigs(appConfigs);
      SaolaConfiguration::Instance().ApplyConfigurations(appConfigs, true);
    }

    for (unsigned int i = 0; i < SaolaConfiguration::Instance().GetppConfigDataSourcePollingInterval() * 10; i++)
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
  
  this->m_worker = new boost::thread([this]() {
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
