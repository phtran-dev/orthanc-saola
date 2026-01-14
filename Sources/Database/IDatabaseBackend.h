#pragma once

#include "../DTO/StableEventDTOCreate.h"
#include "../DTO/StableEventDTOUpdate.h"
#include "../DTO/StableEventDTOGet.h"

#include "../DTO/TransferJobDTOCreate.h"
#include "../DTO/TransferJobDTOGet.h"

#include "../FailedJobFilter.h"
#include "../Pagination.h"

#include <list>
#include <string>
#include <boost/noncopyable.hpp>
#include <ctime>

namespace Saola
{
  class IDatabaseBackend : public boost::noncopyable
  {
  public:
    virtual ~IDatabaseBackend()
    {
    }

    virtual void Open(const std::string& path) = 0;

    virtual void OpenInMemory() = 0;

    // Event operations
    virtual int64_t AddEvent(const StableEventDTOCreate& obj) = 0;

    virtual bool DeleteEventByIds(const std::list<int64_t>& ids) = 0;

    virtual bool UpdateEvent(const StableEventDTOUpdate& obj) = 0;

    virtual bool ResetEvents(const std::list<int64_t>& ids) = 0;

    virtual bool GetById(int64_t id, StableEventDTOGet& result) = 0;

    virtual bool GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results) = 0;

    virtual void FindAll(const Pagination& page, std::list<StableEventDTOGet>& results) = 0;

    virtual void FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results) = 0;

    virtual void FindByAppTypeInRetryLessThan(const std::list<std::string>& appType, bool included, int retry, int limit, std::list<StableEventDTOGet>& results) = 0;

    virtual void Dequeue(const std::list<std::string>& appTypes, bool included, int retry, int limit, const std::string& owner, std::list<StableEventDTOGet>& results) = 0;

    // Transfer Job operations
    virtual void SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result) = 0;

    virtual bool ResetFailedJob(const std::list<std::string>& ids) = 0;

    virtual bool DeleteTransferJobByIds(const std::list<std::string>& ids) = 0;

    virtual bool DeleteTransferJobsByQueueId(int64_t id) = 0;

    virtual bool GetById(const std::string& id, TransferJobDTOGet& result) = 0;

    virtual bool GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results) = 0;

    virtual bool GetTransferJobsByByQueueIds(const std::list<int64_t>& ids, std::list<TransferJobDTOGet>& results) = 0;
  };
}
