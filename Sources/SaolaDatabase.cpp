#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include "Config/SaolaConfiguration.h"
#include "Database/SQLiteDatabaseBackend.h"
#include "Database/RQLiteDatabaseBackend.h"

#include <Logging.h>

void SaolaDatabase::Initialize()
{
  SaolaConfiguration& config = SaolaConfiguration::Instance();
  std::string driver = config.GetDatabaseDriver();

  if (driver == "rqlite")
  {
      backend_.reset(new Saola::RQLiteDatabaseBackend());
  }
  else
  {
      backend_.reset(new Saola::SQLiteDatabaseBackend()); 
  }
}


void SaolaDatabase::Open(const std::string &path)
{
  Initialize(); // Create backend
  if (backend_)
  {
    backend_->Open(path);
  }
}

void SaolaDatabase::OpenInMemory()
{
  Initialize();
  if (backend_)
  {
    backend_->OpenInMemory();
  }
}


int64_t SaolaDatabase::AddEvent(const StableEventDTOCreate &obj)
{
  if (backend_)
  {
    return backend_->AddEvent(obj);
  }
  return -1;
}

bool SaolaDatabase::DeleteEventByIds(const std::list<int64_t> &ids)
{
  if (backend_)
  {
    return backend_->DeleteEventByIds(ids);
  }
  return false;
}

bool SaolaDatabase::UpdateEvent(const StableEventDTOUpdate &obj)
{
  if (backend_)
  {
    return backend_->UpdateEvent(obj);
  }
  return false;
}

bool SaolaDatabase::ResetEvents(const std::list<int64_t> &ids)
{
  if (backend_)
  {
    return backend_->ResetEvents(ids);
  }
  return false;
}

bool SaolaDatabase::GetById(int64_t id, StableEventDTOGet &result)
{
  if (backend_)
  {
     return backend_->GetById(id, result);
  }
  return false;
}

bool SaolaDatabase::GetByIds(const std::list<int64_t> &ids, std::list<StableEventDTOGet> &results)
{
  if (backend_)
  {
    return backend_->GetByIds(ids, results);
  }
  return false;
}

void SaolaDatabase::FindAll(const Pagination &page, const StableEventQueuesFilter &filter, std::list<StableEventDTOGet> &results)
{
  if (backend_)
  {
    backend_->FindAll(page, filter, results);
  }
}

void SaolaDatabase::FindByRetryLessThan(int retry, std::list<StableEventDTOGet> &results)
{
  if (backend_)
  {
    backend_->FindByRetryLessThan(retry, results);
  }
}

void SaolaDatabase::FindByAppTypeInRetryLessThan(const std::list<std::string> &appType, bool included, int retry, int limit, std::list<StableEventDTOGet> &results)
{
  if (backend_)
  {
    backend_->FindByAppTypeInRetryLessThan(appType, included, retry, limit, results);
  }
}

void SaolaDatabase::Dequeue(const std::list<std::string> &appTypes, bool included, int retry, int limit, const std::string &owner, std::list<StableEventDTOGet> &results)
{
  if (backend_)
  {
    backend_->Dequeue(appTypes, included, retry, limit, owner, results);
  }
}

void SaolaDatabase::SaveTransferJob(const TransferJobDTOCreate &dto, TransferJobDTOGet &result)
{
  if (backend_)
  {
    backend_->SaveTransferJob(dto, result);
  }
}

bool SaolaDatabase::ResetFailedJob(const std::list<std::string> &ids)
{
  if (backend_)
  {
    return backend_->ResetFailedJob(ids);
  }
  return false;
}

bool SaolaDatabase::DeleteTransferJobByIds(const std::list<std::string> &ids)
{
  if (backend_)
  {
    return backend_->DeleteTransferJobByIds(ids);
  }
  return false;
}

bool SaolaDatabase::DeleteTransferJobsByQueueId(int64_t id)
{
  if (backend_)
  {
    return backend_->DeleteTransferJobsByQueueId(id);
  }
  return false;
}

bool SaolaDatabase::GetById(const std::string &id, TransferJobDTOGet &result)
{
  if (backend_)
  {
    return backend_->GetById(id, result);
  }
  return false;
}

bool SaolaDatabase::GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet> &results)
{
  if (backend_)
  {
    return backend_->GetTransferJobsByByQueueId(id, results);
  }
  return false;
}

bool SaolaDatabase::GetTransferJobsByByQueueIds(const std::list<int64_t> &ids, std::list<TransferJobDTOGet> &results)
{
  if (backend_)
  {
    return backend_->GetTransferJobsByByQueueIds(ids, results);
  }
  return false;
}

SaolaDatabase &SaolaDatabase::Instance()
{
  static SaolaDatabase instance;
  return instance;
}
