#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include <Logging.h>

#include <EmbeddedResources.h>
#include <SQLite/Transaction.h>
#include <boost/algorithm/string/join.hpp>

void SaolaDatabase::AddFileInternal(const std::string& path,
                                    const std::time_t time,
                                    const uintmax_t size,
                                    bool isDicom,
                                    const std::string& instanceId)
{
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                       "INSERT INTO Files VALUES(?, ?, ?, ?, ?)");
  statement.BindString(0, path);
  statement.BindInt64(1, time);
  statement.BindInt64(2, size);
  statement.BindInt64(3, isDicom);
  statement.BindString(4, instanceId);
  statement.Run();

  transaction.Commit();
}


void SaolaDatabase::Initialize()
{
  {
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    if (!db_.DoesTableExist("StableEventQueues"))
    {
      std::string sql;
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_DATABASE);
      db_.Execute(sql);
    }

    transaction.Commit();
  }
    
  // Performance tuning of SQLite with PRAGMAs
  // http://www.sqlite.org/pragma.html
  db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
  db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
  db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
  db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
}


void SaolaDatabase::Open(const std::string& path)
{
  boost::mutex::scoped_lock lock(mutex_);
  db_.Open(path);
  Initialize();
}
  

void SaolaDatabase::OpenInMemory()
{
  boost::mutex::scoped_lock lock(mutex_);
  db_.OpenInMemory();
  Initialize();
}
  
bool SaolaDatabase::GetById(int64_t id, StableEventDTOGet& result)
{
boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT id, iuid, resource_id, resource_type, app_id, delay_sec, retry, failed_reason, creation_time FROM StableEventQueues WHERE id=?");
  statement.BindInt(0, id);
  bool ok = false;
  while (statement.Step())
  {
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.delay_sec_ = statement.ColumnInt(5);
    result.retry_ = statement.ColumnInt(6);
    result.failed_reason_ = statement.ColumnString(7);
    result.creation_time_ = statement.ColumnString(8);
    ok = true;
  }
        
  transaction.Commit();
  return ok;
}


void SaolaDatabase::FindAll(const Pagination& page, std::list<StableEventDTOGet>& results)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  std::string sql = "SELECT * FROM StableEventQueues ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);

  Orthanc::SQLite::Statement statement(db_, sql);

  while (statement.Step())
  {
    StableEventDTOGet r(statement.ColumnInt64(0),
                        statement.ColumnString(1),
                        statement.ColumnString(2),
                        statement.ColumnString(3),
                        statement.ColumnString(4),
                        statement.ColumnInt(5),
                        statement.ColumnInt(6),
                        statement.ColumnString(7),
                        statement.ColumnString(8));

    results.push_back(r);
  }
        
  transaction.Commit();
}

void SaolaDatabase::FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT * FROM StableEventQueues WHERE retry <= ? ORDER BY retry DESC");
  
  statement.BindInt(0, retry);

  while (statement.Step())
  {
    StableEventDTOGet r(statement.ColumnInt64(0),
                        statement.ColumnString(1),
                        statement.ColumnString(2),
                        statement.ColumnString(3),
                        statement.ColumnString(4),
                        statement.ColumnInt(5),
                        statement.ColumnInt(6),
                        statement.ColumnString(7),
                        statement.ColumnString(8));

    results.push_back(r);
  }
        
  transaction.Commit();
}

int64_t SaolaDatabase::AddEvent(const StableEventDTOCreate& obj)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                         "INSERT INTO StableEventQueues (iuid, resource_id, resource_type, app_id, creation_time) VALUES(?, ?, ?, ?, ?)");
    statement.BindString(0, obj.iuid_);
    statement.BindString(1, obj.resource_id_);
    statement.BindString(2, obj.resouce_type_);
    statement.BindString(3, obj.app_id_);
    statement.BindString(4, boost::posix_time::to_iso_string(GetNow()));
    statement.Run();
  }
  
  transaction.Commit();
  return db_.GetLastInsertRowId();
}

bool SaolaDatabase::DeleteEventByIds(const std::list<int64_t>& ids)
{
  std::list<std::string> ids_;
  for (const auto& id : ids)
  {
    ids_.push_back(std::to_string(id));
  }
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  {
    Orthanc::SQLite::Statement statement(db_, "DELETE FROM StableEventQueues WHERE id IN (?)");
    statement.BindString(0, boost::algorithm::join(ids_, ","));
    ok = statement.Run();
  }
  
  transaction.Commit();
  return ok;
}

bool SaolaDatabase::DeleteEventByIds(const std::string& ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  {
    std::string sql = "DELETE FROM StableEventQueues WHERE id IN (" + ids + ")";

    Orthanc::SQLite::Statement statement(db_, sql);
    ok = statement.Run();
  }
  
  transaction.Commit();
  return ok;
}

bool SaolaDatabase::UpdateEvent(const StableEventDTOUpdate& obj)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                         "UPDATE StableEventQueues SET failed_reason=?, retry=? WHERE id=?");
    statement.BindString(0, obj.failed_reason_);
    statement.BindInt(1, obj.retry_);
    statement.BindInt64(2, obj.id_);
    statement.Run();
  }
  
  transaction.Commit();
  return true;
}

bool SaolaDatabase::ResetEvents()
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                         "UPDATE StableEventQueues SET failed_reason=?, retry=?");
    statement.BindString(0, "Reset");
    statement.BindInt(1, 0);
    statement.Run();
  }
  
  transaction.Commit();
  return true;
}

void SaolaDatabase::SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  
  std::list<TransferJobDTOGet> existings;
  {
    Orthanc::SQLite::Statement statement(db_, "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=? LIMIT 1");
    statement.BindString(0, dto.id_);
    while (statement.Step())
    {
      TransferJobDTOGet r(statement.ColumnString(0), statement.ColumnInt64(1), statement.ColumnString(2), statement.ColumnString(3));

      existings.push_back(r);
    }
  }
  if (existings.empty())
  {
    Orthanc::SQLite::Statement statement(db_, "INSERT INTO TransferJobs (id, queue_id, last_updated_time, creation_time) VALUES(?, ?, ?, ?)");
    statement.BindString(0, dto.id_);
    statement.BindInt64(1, dto.queue_id_);
    statement.BindString(2, boost::posix_time::to_iso_string(GetNow()));
    statement.BindString(3, boost::posix_time::to_iso_string(GetNow()));
    statement.Run();

    result.last_updated_time_ = boost::posix_time::to_iso_string(GetNow());
    result.creation_time_ = boost::posix_time::to_iso_string(GetNow());
  }
  else
  {
    Orthanc::SQLite::Statement statement(db_, "UPDATE TransferJobs SET queue_id=?, last_updated_time=? WHERE id=?");
    statement.BindString(0, boost::posix_time::to_iso_string(GetNow()));
    statement.BindInt64(1, dto.queue_id_);
    statement.BindString(2, dto.id_);
    statement.Run();


    result.last_updated_time_ = boost::posix_time::to_iso_string(GetNow());
    result.creation_time_ = existings.front().creation_time_;
  }
  
  transaction.Commit();

  result.id_ = dto.id_;
}




// void SaolaDatabase::FindAll(const Pagination& page, const FailedJobFilter& filter, std::list<FailedJobDTOGet>& results)
// {
//   boost::mutex::scoped_lock lock(mutex_);
    
//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();

//   std::string sql = "SELECT * FROM FailedJobs ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);

//   if (filter.min_retry_ >= 0)
//   {
//     sql = "SELECT * FROM FailedJobs WHERE retry >= " + std::to_string(filter.min_retry_) + " ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);
//   }
//   else if (filter.max_retry_ >= 0)
//   {
//     sql = "SELECT * FROM FailedJobs WHERE retry <= " + std::to_string(filter.max_retry_) + " ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);
//   }

//   Orthanc::SQLite::Statement statement(db_, sql);

//   while (statement.Step())
//   {
//     FailedJobDTOGet r(statement.ColumnString(0),
//                       statement.ColumnString(1),
//                       statement.ColumnInt(2),
//                       statement.ColumnString(3),
//                       statement.ColumnString(4));

//     results.push_back(r);
//   }
        
//   transaction.Commit();
// }

bool SaolaDatabase::ResetFailedJob(const std::list<std::string>& ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  // By default reset all if not passing list of ids
  std::string sql = "UPDATE FailedJobs SET retry = 0";

  // If list of ids is not empty
  if (!ids.empty())
  {
    std::string str = boost::algorithm::join(ids, "\",\"");
    sql = "UPDATE FailedJobs SET retry = 0 WHERE id IN (\"" + str + "\")";
  }

  Orthanc::SQLite::Statement statement(db_, sql);
  bool ok = statement.Run();

  transaction.Commit();
  return ok;
}

bool SaolaDatabase::DeleteTransferJobByIds(const std::list<std::string>& ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  {
    std::string sql = "DELETE FROM TransferJobs WHERE id IN (\"" + boost::algorithm::join(ids, "\",\"") + "\")";
    LOG(INFO) << "[DeleteTransferJobByIds][SQL] sql=" << sql;

    Orthanc::SQLite::Statement statement(db_, sql);
    ok = statement.Run();
  }
  
  transaction.Commit();
  return ok;
}

bool SaolaDatabase::DeleteTransferJobsByQueueId(int64_t id)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "DELETE FROM TransferJobs WHERE queue_id=?");
  statement.BindInt64(0, id);
  statement.Run();
  
  transaction.Commit();
  return true;
}


bool SaolaDatabase::GetById(const std::string& id, TransferJobDTOGet& result)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id = ?");
  statement.BindString(0, id);
  bool ok = false;
  while (statement.Step())
  {
    result.id_ = statement.ColumnString(0);
    result.queue_id_ = statement.ColumnInt64(1);
    result.last_updated_time_ = statement.ColumnString(2);
    result.creation_time_ = statement.ColumnString(3);
    ok = true;
  }
        
  transaction.Commit(); 
  return ok;
}

bool SaolaDatabase::GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE queue_id=?");
  statement.BindInt64(0, id);
  bool ok = false;
  while (statement.Step())
  {
    TransferJobDTOGet result;
    result.id_ = statement.ColumnString(0);
    result.queue_id_ = statement.ColumnInt64(1);
    result.last_updated_time_ = statement.ColumnString(2);
    result.creation_time_ = statement.ColumnString(3);
    results.push_back(result);
    ok = true;
  }
  transaction.Commit(); 

  return ok; 
}


SaolaDatabase& SaolaDatabase::Instance()
{
  static SaolaDatabase instance;
  return instance;
}
