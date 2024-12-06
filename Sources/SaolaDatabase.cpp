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
                                         "INSERT INTO StableEventQueues (iuid, resource_id, resource_type, app, creation_time) VALUES(?, ?, ?, ?, ?)");
    statement.BindString(0, obj.iuid_);
    statement.BindString(1, obj.resource_id_);
    statement.BindString(2, obj.resouce_type_);
    statement.BindString(3, obj.app_);
    statement.BindString(4, boost::posix_time::to_iso_string(GetNow()));
    statement.Run();
  }
  
  transaction.Commit();
  return db_.GetLastInsertRowId();
}

bool SaolaDatabase::DeleteEventByIds(const std::list<int> ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  std::list<std::string> ids_;
  std::transform(ids.begin(), ids.end(), ids_.begin(), [](int id) -> std::string {return std::to_string(id);});

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

void SaolaDatabase::SaveFailedJob(const FailedJobDTOCreate& dto, FailedJobDTOGet& result)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  
  std::list<FailedJobDTOGet> existings;
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE, "SELECT id, content, retry, last_updated_time, creation_time FROM FailedJobs WHERE id=? LIMIT 1");
    statement.BindString(0, dto.id_);
    while (statement.Step())
    {
      FailedJobDTOGet r(statement.ColumnString(0),
                        statement.ColumnString(1),
                        statement.ColumnInt(2),
                        statement.ColumnString(3),
                        statement.ColumnString(4));

      existings.push_back(r);
    }
  }

  if (existings.empty())
  {
    Orthanc::SQLite::Statement statement(db_, "INSERT INTO FailedJobs (id, content, last_updated_time, creation_time) VALUES(?, ?, ?, ?)");
    statement.BindString(0, dto.id_);
    statement.BindString(1, dto.content_);
    statement.BindString(2, boost::posix_time::to_iso_string(GetNow()));
    statement.BindString(3, boost::posix_time::to_iso_string(GetNow()));
    statement.Run();

    result.id_ = dto.id_;
    result.content_ = dto.content_;
    result.last_updated_time_ = boost::posix_time::to_iso_string(GetNow());
    result.creation_time_ = boost::posix_time::to_iso_string(GetNow());
    result.retry_ = 0;
  }
  else
  {
    Orthanc::SQLite::Statement statement(db_, "UPDATE FailedJobs SET content=?, retry=?, last_updated_time=? WHERE id=?");
    statement.BindString(0, dto.content_);
    statement.BindInt64(1, existings.front().retry_ + 1);
    statement.BindString(2, boost::posix_time::to_iso_string(GetNow()));
    statement.BindString(3, dto.id_);
    statement.Run();


    result.last_updated_time_ = boost::posix_time::to_iso_string(GetNow());
    result.creation_time_ = existings.front().creation_time_;
    result.retry_ = existings.front().retry_ + 1;
  }
  
  transaction.Commit();

  result.id_ = dto.id_;
  result.content_ = dto.content_;
}


void SaolaDatabase::FindAll(const Pagination& page, const FailedJobFilter& filter, std::list<FailedJobDTOGet>& results)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  std::string sql = "SELECT * FROM FailedJobs ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);

  if (filter.min_retry_ >= 0)
  {
    sql = "SELECT * FROM FailedJobs WHERE retry >= " + std::to_string(filter.min_retry_) + " ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);
  }
  else if (filter.max_retry_ >= 0)
  {
    sql = "SELECT * FROM FailedJobs WHERE retry <= " + std::to_string(filter.max_retry_) + " ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);
  }

  Orthanc::SQLite::Statement statement(db_, sql);

  while (statement.Step())
  {
    FailedJobDTOGet r(statement.ColumnString(0),
                      statement.ColumnString(1),
                      statement.ColumnInt(2),
                      statement.ColumnString(3),
                      statement.ColumnString(4));

    results.push_back(r);
  }
        
  transaction.Commit();
}

bool SaolaDatabase::ResetFailedJob(const std::list<std::string>& ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  std::string str = boost::algorithm::join(ids, "\",\"");


  bool ok = true;
  {
    std::string sql = "UPDATE FailedJobs SET retry = 0 WHERE id IN (\"" + str + "\")";
    LOG(INFO) << "PHONG ResetFailedJob sql=" << sql;

    Orthanc::SQLite::Statement statement(db_, sql);
    ok = statement.Run();
  }
  
  transaction.Commit();
  return ok;
}

bool SaolaDatabase::DeleteFailedJobByIds(const std::string& ids)
{
  boost::mutex::scoped_lock lock(mutex_);
    
  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  {
    std::string sql = "DELETE FROM FailedJobs WHERE id IN (\"" + ids + "\")";

    Orthanc::SQLite::Statement statement(db_, sql);
    ok = statement.Run();
  }
  
  transaction.Commit();
  return ok;
}


SaolaDatabase& SaolaDatabase::Instance()
{
  static SaolaDatabase instance;
  return instance;
}
