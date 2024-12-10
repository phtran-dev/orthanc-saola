
#include "SaolaConfiguration.h"

#include "FailedJobScheduler.h"
#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include "FailedJobDTOGet.h"
#include "FailedJobFilter.h"

#include "Constants.h"

#include <Toolbox.h>
#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>


static void ProcessJob(const FailedJobDTOGet& job)
{
  Json::Value response;
  bool ok = OrthancPlugins::RestApiGet(response, "/jobs/" + job.id_, false) && !response.empty();

  if (ok)
  {
    // Resubmit job
    if (response["State"].asString() == "Failure")
    {
      Json::Value result;
      ok = OrthancPlugins::RestApiPost(result, "/jobs/" + job.id_ + "/resubmit", std::string(""), false);
    }
    else if (response["State"].asString() == "Success")
    {
      SaolaDatabase::Instance().DeleteFailedJobByIds(std::list<std::string>{job.id_});
    }
  }
  else
  {
    // Submit a new job
    try
    {
      Json::Value json;
      LOG(INFO) << "[ProcessFailedJobs] job content=" << job.content_;
      OrthancPlugins::ReadJson(json, job.content_);
      LOG(INFO) << "[ProcessFailedJobs] After parse = " << json.toStyledString();

      if (!job.content_.empty() && OrthancPlugins::ReadJson(json, job.content_) && !json.empty() && json.isMember("Content")
          && json["Content"].isMember("Resources")
          && json.isMember("Type") && json["Type"].asString() == "PushTransfer")
      {
        LOG(INFO) << "[ProcessFailedJobs] PushTransfer Again";
        Json::Value body = json["Content"];
        LOG(INFO) << "[ProcessFailedJobs] PushTransfer Again body=" << body.toStyledString();
        Json::Value result;
        if (OrthancPlugins::RestApiPost(result, "/push/transfer", body, false))
        {
          // Delete Old Job
          ok = SaolaDatabase::Instance().DeleteFailedJobByIds(std::list<std::string>{job.id_});
        }
        else
        {
          ok = false;
        }
      }
      else
      {
        ok = false;
      }    
    }
    catch (std::exception& e)
    {
      LOG(INFO) << "[RetryFailedJobs] Parsing json:" << job.content_ << "Caught exception: " << e.what();
      ok = false;
    }
  }
    
  if (!ok)
  {
    // Increase retry
    FailedJobDTOCreate dto;
    dto.id_ = job.id_;
    dto.content_ = job.content_;

    FailedJobDTOGet result;
    SaolaDatabase::Instance().SaveFailedJob(dto, result);
  }

}

FailedJobScheduler& FailedJobScheduler::Instance()
{
  static FailedJobScheduler instance;
  return instance;
}

FailedJobScheduler::~FailedJobScheduler()
{
  if (this->m_state == State_Running)
  {
    OrthancPlugins::LogError("FailedJobScheduler::Stop() should have been manually called");
    Stop();
  }
}

void FailedJobScheduler::MonitorDatabase()
{
  LOG(INFO) << "[FailedJobScheduler::MonitorDatabase] Start monitoring ...";


  Pagination page;
  page.sort_by_ = "retry";
  FailedJobFilter filter(-1, SaolaConfiguration::Instance().GetMaxRetry());
  std::list<FailedJobDTOGet> jobs;


  SaolaDatabase::Instance().FindAll(page, filter, jobs);

  for (const auto& job : jobs)
  {
    ProcessJob(job);
  }

}


void FailedJobScheduler::Start()
{
  if (this->m_state != State_Setup)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
  }
  this->m_state = State_Running;
  const auto intervalSeconds = 10;
  this->m_worker = new std::thread([this, intervalSeconds]() {
    while (this->m_state == State_Running)
    {
      this->MonitorDatabase();
      for (unsigned int i = 0; i < intervalSeconds * 10; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  });
}

void FailedJobScheduler::Stop()
{
  if (this->m_state == State_Running)
  {
    this->m_state = State_Done;
    if (this->m_worker->joinable())
        this->m_worker->join();
    delete this->m_worker;
  }
}

    


