#pragma once

#include "IDatabaseBackend.h"
#include "RQLite.h"

namespace Saola
{
  class RQLiteDatabaseBackend : public IDatabaseBackend
  {
  public:
    virtual void Open(const std::string& path) override;

    virtual void OpenInMemory() override;

    virtual int64_t AddEvent(const StableEventDTOCreate& obj) override;

    virtual bool DeleteEventByIds(const std::list<int64_t>& ids) override;

    virtual bool UpdateEvent(const StableEventDTOUpdate& obj) override;

    virtual bool ResetEvents(const std::list<int64_t>& ids) override;

    virtual bool GetById(int64_t id, StableEventDTOGet& result) override;

    virtual bool GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results) override;

    virtual void FindAll(const Pagination& page, const StableEventQueuesFilter& filter, std::list<StableEventDTOGet>& results) override;

    virtual void FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results) override;

    virtual void FindByAppTypeInRetryLessThan(const std::list<std::string>& appType, bool included, int retry, int limit, std::list<StableEventDTOGet>& results) override;

    virtual void Dequeue(const std::list<std::string>& appTypes, bool included, int retry, int limit, const std::string& owner, std::list<StableEventDTOGet>& results) override;

    virtual void SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result) override;

    virtual bool ResetFailedJob(const std::list<std::string>& ids) override;

    virtual bool DeleteTransferJobByIds(const std::list<std::string>& ids) override;

    virtual bool DeleteTransferJobsByQueueId(int64_t id) override;

    virtual bool GetById(const std::string& id, TransferJobDTOGet& result) override;

    virtual bool GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results) override;

    virtual bool GetTransferJobsByByQueueIds(const std::list<int64_t>& ids, std::list<TransferJobDTOGet>& results) override;

  private:
    std::unique_ptr<rqlite::RqliteClient> rqliteClient_;
    void Initialize();
  };
}
