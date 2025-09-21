#include "JobHandler.h"

#include "../Config/SaolaConfiguration.h"
#include "../SaolaDatabase.h"

#include "../DTO/StableEventDTOUpdate.h"

#include "../Cache/InMemoryJobCache.h"

#include "../Notification/Notification.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>

#include <boost/algorithm/string/join.hpp>

namespace Saola
{
  constexpr const char *ERROR_MESSAGE = "ErrorMessage";
  constexpr const char *ERROR_DETAIL = "ErrorDetail";

  void OnJobSubmitted(const std::string &jobId)
  {
    LOG(INFO) << "[Saola::OnJobSubmitted] new job submitted jobId=" << jobId;
    if (SaolaConfiguration::Instance().EnableInMemJobCache())
    {
      Json::Value job;
      OrthancPlugins::RestApiGet(job, "/jobs/" + jobId, false);
      if (!job.isNull() && !job.empty() && job.isMember("Type") && SaolaConfiguration::Instance().GetInMemJobTypes().find(job["Type"].asString()) != SaolaConfiguration::Instance().GetInMemJobTypes().end())
      {
        LOG(INFO) << "[Saola::OnJobSubmitted] Cache job=" << job.toStyledString();
        InMemoryJobCache::Instance().Insert(jobId, job);
      }
    }
  }

  void OnJobSuccess(const std::string &jobId)
  {
    LOG(INFO) << "[OnJobSuccess] Processing JOB jobId=" << jobId;
    InMemoryJobCache::Instance().Delete(jobId);
    try
    {
      TransferJobDTOGet dto;
      if (SaolaDatabase::Instance().GetById(jobId, dto))
      {
        LOG(INFO) << "[OnJobSuccess] Deleting JOB " << dto.ToJsonString();
        SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
        SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.queue_id_});
      }
      else
      {
        LOG(INFO) << "[OnJobSuccess] Cannot find JOB jobId=" << jobId;
      }
    }
    catch (const std::exception &e)
    {
      LOG(ERROR) << "[OnJobSuccess] ERROR JOB " << jobId << " Exception: " << e.what();
    }
    catch (...)
    {
      LOG(ERROR) << "[OnJobSuccess] ERROR JOB " << jobId << " Undetermined exception";
    }
  }

  void OnJobFailure(const std::string &jobId)
  {
    LOG(INFO) << "[OnJobFailure] Processing jobId=" << jobId;
    InMemoryJobCache::Instance().Delete(jobId);
    try
    {
      TransferJobDTOGet dto;
      if (SaolaDatabase::Instance().GetById(jobId, dto))
      {
        LOG(INFO) << "[OnJobFailure] Deleting job " << dto.ToJsonString();
        SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
        StableEventDTOGet dtoGet;
        if (SaolaDatabase::Instance().GetById(dto.queue_id_, dtoGet))
        {
          dtoGet.retry_ += 1;
          LOG(INFO) << "[OnJobFailure] Updating queue " << dtoGet.ToJsonString();
          SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dtoGet.id_, "Callback OnJobFailure triggered", dtoGet.retry_, Saola::GetNextXSecondsFromNowInString(dtoGet.delay_sec_).c_str()));
          Json::Value notification;
          notification[ERROR_DETAIL] = dto.ToJsonString();
          notification[ERROR_MESSAGE] = "Job Failure for queue_id=" + std::to_string(dto.queue_id_) + ", jobId=" + jobId + ", increasing retry to " + std::to_string(dtoGet.retry_);
          Notification::Instance().SendMessage(notification);
        }
        else
        {
          LOG(INFO) << "[OnJobFailure] ERROR JOB " << jobId << " cannot find QUEUE";
        }
      }
      else
      {
        LOG(INFO) << "[OnJobFailure] ERROR cannot find JOB " << jobId;
      }
    }
    catch (const std::exception &e)
    {
      LOG(ERROR) << "[OnJobFailure] ERROR JOB " << jobId << " exception: " << e.what();
    }
    catch (...)
    {
      LOG(ERROR) << "[OnJobFailure] ERROR JOB " << jobId << " undetermined exception";
    }
  }
}
