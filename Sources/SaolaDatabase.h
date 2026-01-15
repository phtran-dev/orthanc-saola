#pragma once

#include "DTO/StableEventDTOCreate.h"
#include "DTO/StableEventDTOUpdate.h"
#include "DTO/StableEventDTOGet.h"

#include "DTO/TransferJobDTOCreate.h"
#include "DTO/TransferJobDTOGet.h"

#include "FailedJobFilter.h"

#include "Pagination.h"

#include <list>

#include <OrthancFramework.h>  // To have ORTHANC_ENABLE_SQLITE defined
#include "Database/IDatabaseBackend.h"
#include <memory>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>



class SaolaDatabase : public boost::noncopyable
{
private:
  std::unique_ptr<Saola::IDatabaseBackend> backend_;
  
  void Initialize();

  void AddFileInternal(const std::string& path,
                       const std::time_t time,
                       const uintmax_t size,
                       bool isDicom,
                       const std::string& instanceId);

public:

  static SaolaDatabase& Instance();

  void Open(const std::string& path);

  void OpenInMemory();  // For unit tests

  int64_t AddEvent(const StableEventDTOCreate& obj);

  bool DeleteEventByIds(const std::list<int64_t>& ids);

  bool UpdateEvent(const StableEventDTOUpdate& obj);

  bool ResetEvents(const std::list<int64_t>& ids);

  bool GetById(int64_t id, StableEventDTOGet& result);

  bool GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results);

  void FindAll(const Pagination& page, std::list<StableEventDTOGet>& results);

  void FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results);

  void FindByAppTypeInRetryLessThan(const std::list<std::string>& appType, bool included, int retry, int limit, std::list<StableEventDTOGet>& results);

  void Dequeue(const std::list<std::string>& appTypes, bool included, int retry, int limit, const std::string& owner, std::list<StableEventDTOGet>& results);

  void SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result);

  // void FindAll(const Pagination& page, const FailedJobFilter& filter, std::list<FailedJobDTOGet>& results);

  bool ResetFailedJob(const std::list<std::string>& ids);

  bool DeleteTransferJobByIds(const std::list<std::string>& ids);

  bool DeleteTransferJobsByQueueId(int64_t id);

  bool GetById(const std::string& id, TransferJobDTOGet& result);

  bool GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results);

  bool GetTransferJobsByByQueueIds(const std::list<int64_t>& ids, std::list<TransferJobDTOGet>& results);

};
