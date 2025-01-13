#include "JobHandler.h"

#include "SaolaConfiguration.h"
#include "SaolaDatabase.h"

#include "StableEventDTOUpdate.h"

#include <Logging.h>

#include <boost/algorithm/string/join.hpp>

void OnJobSuccess(const std::string &jobId)
{
  LOG(INFO) << "[OnJobSuccess] Processing JOB jobId=" << jobId;
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
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dtoGet.id_, "Callback OnJobFailure triggered", dtoGet.retry_));
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