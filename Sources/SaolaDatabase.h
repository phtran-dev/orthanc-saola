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
#include <SQLite/Connection.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>



class SaolaDatabase : public boost::noncopyable
{
public:
  enum FileStatus
  {
    FileStatus_New,
    FileStatus_Modified,
    FileStatus_AlreadyStored,
    FileStatus_NotDicom
  };

  class IFileVisitor : public boost::noncopyable
  {
  public:
    virtual ~IFileVisitor()
    {
    }

    virtual void VisitInstance(const std::string& path,
                               bool isDicom,
                               const std::string& instanceId) = 0;
  };  


private:
  boost::mutex                 mutex_;
  Orthanc::SQLite::Connection  db_;
  
  void Initialize();

  void AddFileInternal(const std::string& path,
                       const std::time_t time,
                       const uintmax_t size,
                       bool isDicom,
                       const std::string& instanceId);

  SaolaDatabase()
  {}

public:

  static SaolaDatabase& Instance();

  void Open(const std::string& path);

  void OpenInMemory();  // For unit tests

  FileStatus LookupFile(std::string& oldInstanceId,
                        const std::string& path,
                        const std::time_t time,
                        const uintmax_t size);


  // Warning: The visitor is invoked in mutual exclusion, so it
  // shouldn't do lengthy operations
  void Apply(IFileVisitor& visitor);

  // Returns "false" iff. this instance has not been previously
  // registerded using "AddDicomInstance()", which indicates the
  // import of an external DICOM file
  int64_t AddEvent(const StableEventDTOCreate& obj);

  bool DeleteEventByIds(const std::list<int64_t>& ids);

  bool DeleteEventByIds(const std::string& ids);

  bool UpdateEvent(const StableEventDTOUpdate& obj);

  bool ResetEvents(const std::list<int64_t>& ids);

  bool GetById(int64_t id, StableEventDTOGet& result);

  bool GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results);

  bool GetByIds(const std::string& ids, std::list<StableEventDTOGet>& results);

  void FindAll(const Pagination& page, std::list<StableEventDTOGet>& results);

  void FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results);

  void FindByAppTypeInRetryLessThan(const std::list<std::string>& appType, bool included, int retry, int limit, std::list<StableEventDTOGet>& results);

  void SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result);

  // void FindAll(const Pagination& page, const FailedJobFilter& filter, std::list<FailedJobDTOGet>& results);

  bool ResetFailedJob(const std::list<std::string>& ids);

  bool DeleteTransferJobByIds(const std::list<std::string>& ids);

  bool DeleteTransferJobsByQueueId(int64_t id);

  bool GetById(const std::string& id, TransferJobDTOGet& result);

  bool GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results);

};
