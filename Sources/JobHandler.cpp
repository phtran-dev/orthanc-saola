#include "JobHandler.h"

#include "SaolaConfiguration.h"
#include "SaolaDatabase.h"

#include "StableEventDTOUpdate.h"

#include <Logging.h>

#include <boost/algorithm/string/join.hpp>

void OnJobSuccess(const std::string &jobId)
{
  try
  {
    TransferJobDTOGet dto;
    if (SaolaDatabase::Instance().GetById(jobId, dto))
    {
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
      SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.queue_id_});
    }
    else
    {
      // TODO
    }
  }
  catch (const std::exception &e)
  {
    LOG(ERROR) << "[OnJobSuccess] Exception: " << e.what();
  }
  catch (...)
  {
    LOG(ERROR) << "[OnJobSuccess] Undetermined exception";
  }
}

void OnJobFailure(const std::string &jobId)
{
  try
  {
    TransferJobDTOGet dto;
    if (SaolaDatabase::Instance().GetById(jobId, dto))
    {
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
      StableEventDTOGet dtoGet;
      if (SaolaDatabase::Instance().GetById(dto.queue_id_, dtoGet))
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dtoGet.id_, "Callback OnJobFailure triggered", dtoGet.retry_ + 1));
      }
    }
  }
  catch (const std::exception &e)
  {
    LOG(ERROR) << "[OnJobFailure] Exception: " << e.what();
  }
  catch (...)
  {
    LOG(ERROR) << "[OnJobFailure] Undetermined exception";
  }
}