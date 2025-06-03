#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include <Logging.h>

#include <EmbeddedResources.h>
#include <SQLite/Transaction.h>
#include <boost/algorithm/string/join.hpp>

void SaolaDatabase::AddFileInternal(const std::string &path,
                                    const std::time_t time,
                                    const uintmax_t size,
                                    bool isDicom,
                                    const std::string &instanceId)
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

void SaolaDatabase::Open(const std::string &path)
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

bool SaolaDatabase::GetById(int64_t id, StableEventDTOGet &result)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues WHERE id=?");
  statement.BindInt(0, id);
  bool ok = false;
  while (statement.Step())
  {
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.app_type_ = statement.ColumnString(5);
    result.delay_sec_ = statement.ColumnInt(6);
    result.retry_ = statement.ColumnInt(7);
    result.failed_reason_ = statement.ColumnString(8);
    result.last_updated_time_ = statement.ColumnString(9);
    result.creation_time_ = statement.ColumnString(10);
    ok = true;
  }

  transaction.Commit();
  return ok;
}

// bool SaolaDatabase::GetByIds(const std::list<int64_t> &ids, std::list<StableEventDTOGet> &results)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   std::list<std::string> ids_;
//   for (const auto &id : ids)
//   {
//     ids_.push_back(std::to_string(id));
//   }

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();
//   Orthanc::SQLite::Statement statement(db_, "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues WHERE id IN (" + boost::algorithm::join(ids_, ",") + ")");
//   bool ok = false;
//   while (statement.Step())
//   {
//     StableEventDTOGet result;
//     result.id_ = statement.ColumnInt64(0);
//     result.iuid_ = statement.ColumnString(1);
//     result.resource_id_ = statement.ColumnString(2);
//     result.resource_type_ = statement.ColumnString(3);
//     result.app_id_ = statement.ColumnString(4);
//     result.app_type_ = statement.ColumnString(5);
//     result.delay_sec_ = statement.ColumnInt(6);
//     result.retry_ = statement.ColumnInt(7);
//     result.failed_reason_ = statement.ColumnString(8);
//     result.last_updated_time_ = statement.ColumnString(9);
//     result.creation_time_ = statement.ColumnString(10);

//     results.push_back(result);
//     ok = true;
//   }

//   transaction.Commit();
//   return ok;
// }


bool SaolaDatabase::GetByIds(const std::list<int64_t> &ids, std::list<StableEventDTOGet> &results)
{
  boost::mutex::scoped_lock lock(mutex_);

  if (ids.empty()) {
    return false;
  }

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  
  // Create the parameterized query with the right number of placeholders
  std::string query = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, "
                      "delay_sec, retry, failed_reason, last_updated_time, creation_time "
                      "FROM StableEventQueues WHERE id IN (";
  
  // Add the right number of parameter placeholders
  std::string placeholders;
  for (size_t i = 0; i < ids.size(); i++) {
    if (i > 0) placeholders += ",";
    placeholders += "?";
  }
  query += placeholders + ")";
  // LOG(INFO) << "SaolaDatabase::GetByIds sql=" << query;
  
  Orthanc::SQLite::Statement statement(db_, query);
  
  int paramIndex = 0; // Many SQLite interfaces start at 1
  for (const auto &id : ids) {
    statement.BindInt64(paramIndex++, id);
  }
  
  bool ok = false;
  while (statement.Step())
  {
    StableEventDTOGet result;
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.app_type_ = statement.ColumnString(5);
    result.delay_sec_ = statement.ColumnInt(6);
    result.retry_ = statement.ColumnInt(7);
    result.failed_reason_ = statement.ColumnString(8);
    result.last_updated_time_ = statement.ColumnString(9);
    result.creation_time_ = statement.ColumnString(10);

    results.push_back(result);
    ok = true;
  }

  transaction.Commit();
  return ok;
}


// void SaolaDatabase::FindAll(const Pagination &page, std::list<StableEventDTOGet> &results)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();

//   std::string sql = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues ORDER BY " + page.sort_by_ + " LIMIT " + std::to_string(page.limit_) + " OFFSET " + std::to_string(page.offset_);

//   Orthanc::SQLite::Statement statement(db_, sql);

//   while (statement.Step())
//   {
//     StableEventDTOGet result;
//     result.id_ = statement.ColumnInt64(0);
//     result.iuid_ = statement.ColumnString(1);
//     result.resource_id_ = statement.ColumnString(2);
//     result.resource_type_ = statement.ColumnString(3);
//     result.app_id_ = statement.ColumnString(4);
//     result.app_type_ = statement.ColumnString(5);
//     result.delay_sec_ = statement.ColumnInt(6);
//     result.retry_ = statement.ColumnInt(7);
//     result.failed_reason_ = statement.ColumnString(8);
//     result.last_updated_time_ = statement.ColumnString(9);
//     result.creation_time_ = statement.ColumnString(10);

//     results.push_back(result);
//   }

//   transaction.Commit();
// }




void SaolaDatabase::FindAll(const Pagination &page, std::list<StableEventDTOGet> &results)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  // For column names, you need to validate them against a whitelist
  // to prevent SQL injection, as you can't parameterize column names
  std::set<std::string> validColumns = {"id", "iuid", "resource_id", "resource_type", 
                                        "app_id", "app_type", "delay_sec", "retry", 
                                        "failed_reason", "last_updated_time", "creation_time"};
  
  // Validate the sort_by column
  std::string sortBy = "id"; // Default sort column
  if (validColumns.find(page.sort_by_) != validColumns.end()) {
    sortBy = page.sort_by_;
  }

  std::string sql = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, "
                    "delay_sec, retry, failed_reason, last_updated_time, creation_time "
                    "FROM StableEventQueues ORDER BY " + sortBy + " LIMIT ? OFFSET ?";

  // LOG(INFO) << "SaolaDatabase::FindAll sql=" << sql << ", limit=" << page.limit_ << ", offset=" << page.offset_;

  Orthanc::SQLite::Statement statement(db_, sql);
  
  // Bind the parameters
  statement.BindInt(0, page.limit_);
  statement.BindInt64(1, page.offset_);

  while (statement.Step())
  {
    StableEventDTOGet result;
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.app_type_ = statement.ColumnString(5);
    result.delay_sec_ = statement.ColumnInt(6);
    result.retry_ = statement.ColumnInt(7);
    result.failed_reason_ = statement.ColumnString(8);
    result.last_updated_time_ = statement.ColumnString(9);
    result.creation_time_ = statement.ColumnString(10);

    results.push_back(result);
  }

  transaction.Commit();
}

void SaolaDatabase::FindByRetryLessThan(int retry, std::list<StableEventDTOGet> &results)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  Orthanc::SQLite::Statement statement(db_, "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues WHERE retry <= ? ORDER BY retry ASC");

  statement.BindInt(0, retry);

  while (statement.Step())
  {
    StableEventDTOGet result;
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.app_type_ = statement.ColumnString(5);
    result.delay_sec_ = statement.ColumnInt(6);
    result.retry_ = statement.ColumnInt(7);
    result.failed_reason_ = statement.ColumnString(8);
    result.last_updated_time_ = statement.ColumnString(9);
    result.creation_time_ = statement.ColumnString(10);

    results.push_back(result);
  }

  transaction.Commit();
}

// void SaolaDatabase::FindByAppTypeInRetryLessThan(const std::list<std::string> &appTypes, bool included, int retry, int limit, std::list<StableEventDTOGet> &results)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();

//   std::string str = boost::algorithm::join(appTypes, "\",\"");

//   std::string sql = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues WHERE app_type IN (\"" + str + "\") AND retry <= ? ORDER BY retry ASC LIMIT " + std::to_string(limit);
//   if (!included)
//   {
//     sql = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, last_updated_time, creation_time FROM StableEventQueues WHERE app_type NOT IN (\"" + str + "\") AND retry <= ? ORDER BY retry ASC LIMIT " + std::to_string(limit);
//   }

//   Orthanc::SQLite::Statement statement(db_, sql);

//   statement.BindInt(0, retry);

//   while (statement.Step())
//   {
//     StableEventDTOGet result;
//     result.id_ = statement.ColumnInt64(0);
//     result.iuid_ = statement.ColumnString(1);
//     result.resource_id_ = statement.ColumnString(2);
//     result.resource_type_ = statement.ColumnString(3);
//     result.app_id_ = statement.ColumnString(4);
//     result.app_type_ = statement.ColumnString(5);
//     result.delay_sec_ = statement.ColumnInt(6);
//     result.retry_ = statement.ColumnInt(7);
//     result.failed_reason_ = statement.ColumnString(8);
//     result.last_updated_time_ = statement.ColumnString(9);
//     result.creation_time_ = statement.ColumnString(10);

//     results.push_back(result);
//   }

//   transaction.Commit();
// }


void SaolaDatabase::FindByAppTypeInRetryLessThan(const std::list<std::string> &appTypes, bool included, int retry, int limit, std::list<StableEventDTOGet> &results)
{
  boost::mutex::scoped_lock lock(mutex_);

  if (appTypes.empty()) {
    return; // No app types to query
  }

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  // Create the base query with placeholders for app types
  std::string baseQuery = "SELECT id, iuid, resource_id, resource_type, app_id, app_type, "
                          "delay_sec, retry, failed_reason, last_updated_time, creation_time "
                          "FROM StableEventQueues WHERE app_type ";

  // Create the IN clause with the right number of placeholders
  std::string inOperator = included ? "IN" : "NOT IN";
  std::string inClause = inOperator + " (";
  
  // Add a parameter placeholder for each app type
  for (size_t i = 0; i < appTypes.size(); i++) {
    inClause += (i > 0) ? ",?" : "?";
  }
  inClause += ")";
  
  // Complete the query with placeholders for retry and limit
  std::string sql = baseQuery + inClause + " AND retry <= ? ORDER BY retry ASC LIMIT ?";
  // LOG(INFO) << "SaolaDatabase::FindByAppTypeInRetryLessThan sql=" << sql;
  
  // Prepare the statement
  Orthanc::SQLite::Statement statement(db_, sql);
  
  // Bind each app type
  int paramIndex = 0; // Starting with 1 for parameter binding
  for (const auto& appType : appTypes) {
    statement.BindString(paramIndex++, appType);
  }
  
  // Bind retry and limit parameters
  statement.BindInt(paramIndex++, retry);  
  statement.BindInt(paramIndex, limit);
  
  // Execute and gather results
  while (statement.Step())
  {
    StableEventDTOGet result;
    result.id_ = statement.ColumnInt64(0);
    result.iuid_ = statement.ColumnString(1);
    result.resource_id_ = statement.ColumnString(2);
    result.resource_type_ = statement.ColumnString(3);
    result.app_id_ = statement.ColumnString(4);
    result.app_type_ = statement.ColumnString(5);
    result.delay_sec_ = statement.ColumnInt(6);
    result.retry_ = statement.ColumnInt(7);
    result.failed_reason_ = statement.ColumnString(8);
    result.last_updated_time_ = statement.ColumnString(9);
    result.creation_time_ = statement.ColumnString(10);

    results.push_back(result);
  }

  transaction.Commit();
}

int64_t SaolaDatabase::AddEvent(const StableEventDTOCreate &obj)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                         "INSERT INTO StableEventQueues (iuid, resource_id, resource_type, app_id, app_type, delay_sec, last_updated_time, creation_time) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    statement.BindString(0, obj.iuid_);
    statement.BindString(1, obj.resource_id_);
    statement.BindString(2, obj.resouce_type_);
    statement.BindString(3, obj.app_id_);
    statement.BindString(4, obj.app_type_);
    statement.BindInt(5, obj.delay_);
    statement.BindString(6, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.BindString(7, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.Run();
  }

  transaction.Commit();
  return db_.GetLastInsertRowId();
}

// bool SaolaDatabase::DeleteEventByIds(const std::list<int64_t> &ids)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   std::list<std::string> ids_;
//   for (const auto &id : ids)
//   {
//     ids_.push_back(std::to_string(id));
//   }

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();
//   bool ok = true;
//   std::string sql = "DELETE FROM StableEventQueues";
//   if (!ids_.empty())
//   {
//     sql += " WHERE id IN (" + boost::algorithm::join(ids_, ",") + ")";
//   }

//   {
//     Orthanc::SQLite::Statement statement(db_, sql);
//     ok = statement.Run();
//   }

//   transaction.Commit();
//   return ok;
// }


bool SaolaDatabase::DeleteEventByIds(const std::list<int64_t> &ids)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;

  if (ids.empty())
  {
    // Delete all rows when no ids are specified
    Orthanc::SQLite::Statement statement(db_, "DELETE FROM StableEventQueues");
    ok = statement.Run();
  }
  else
  {
    // Create SQL with the right number of placeholders for the IN clause
    std::string sql = "DELETE FROM StableEventQueues WHERE id IN (";
    
    // Add the appropriate number of parameter placeholders
    // Add a parameter placeholder for each app type
    for (size_t i = 0; i < ids.size(); i++)
    {
      sql += (i > 0) ? ",?" : "?";
    }
    sql += ")";
    LOG(INFO) << "SaolaDatabase::DeleteEventByIds sql=" << sql;

    // Prepare the statement
    Orthanc::SQLite::Statement statement(db_, sql);
    
    // Bind each ID parameter
    int paramIndex = 0;
    for (const auto &id : ids)
    {
      statement.BindInt64(paramIndex++, id);
    }
    
    // Execute the statement
    ok = statement.Run();
  }

  transaction.Commit();
  return ok;
}


bool SaolaDatabase::UpdateEvent(const StableEventDTOUpdate &obj)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  {
    Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                         "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=? WHERE id=?");
    statement.BindString(0, obj.failed_reason_);
    statement.BindInt(1, obj.retry_);
    statement.BindString(2, obj.last_updated_time_);
    statement.BindInt64(3, obj.id_);
    statement.Run();
  }

  transaction.Commit();
  return true;
}

// bool SaolaDatabase::ResetEvents(const std::list<int64_t> &ids)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   std::list<std::string> ids_;
//   for (const auto &id : ids)
//   {
//     ids_.push_back(std::to_string(id));
//   }

//   std::string sql = "UPDATE StableEventQueues SET failed_reason=?, retry=?,  last_updated_time=?";
//   if (!ids_.empty())
//   {
//     sql = "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=? WHERE id IN (" + boost::algorithm::join(ids_, ",") + ")";
//   }

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();
//   {
//     Orthanc::SQLite::Statement statement(db_, sql);
//     statement.BindString(0, "Reset");
//     statement.BindInt(1, 0);
//     statement.BindString(2, boost::posix_time::to_iso_string(Saola::GetNow()));
//     statement.Run();
//   }

//   transaction.Commit();
//   return true;
// }


bool SaolaDatabase::ResetEvents(const std::list<int64_t> &ids)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  if (ids.empty())
  {
    // Reset all events when no ids are specified
    std::string sql = "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=?";
    LOG(INFO) << "SaolaDatabase::ResetEvents sql=" << sql;
    Orthanc::SQLite::Statement statement(db_, sql);
    statement.BindString(0, "Reset");
    statement.BindInt(1, 0);
    statement.BindString(2, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.Run();
  }
  else
  {
    // Create SQL with placeholders for both update values and the IN clause
    std::string sql = "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=? WHERE id IN (";
    
    // Add the appropriate number of parameter placeholders for IDs
    for (size_t i = 0; i < ids.size(); i++)
    {
      sql += (i > 0) ? ",?" : "?";
    }
    sql += ")";
    LOG(INFO) << "SaolaDatabase::ResetEvents sql=" << sql;
    // Prepare the statement
    Orthanc::SQLite::Statement statement(db_, sql);
    
    // Bind the update field parameters first
    int paramIndex = 0;
    statement.BindString(paramIndex++, "Reset");
    statement.BindInt(paramIndex++, 0);
    statement.BindString(paramIndex++, boost::posix_time::to_iso_string(Saola::GetNow()));
    
    // Then bind each ID for the IN clause
    for (const auto &id : ids)
    {
      statement.BindInt64(paramIndex++, id);
    }
    
    // Execute the statement
    statement.Run();
  }

  transaction.Commit();
  return true;
}

void SaolaDatabase::SaveTransferJob(const TransferJobDTOCreate &dto, TransferJobDTOGet &result)
{
  boost::mutex::scoped_lock lock(mutex_);
  LOG(INFO) << "[SaolaDatabase::SaveTransferJob] Saving jobId=" << dto.id_;

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();

  std::list<TransferJobDTOGet> existings;
  {
    LOG(INFO) << "SaolaDatabase::SaveTransferJob BEGIN sql=" << "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=? LIMIT 1";
    Orthanc::SQLite::Statement statement(db_, "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=? LIMIT 1");
    statement.BindString(0, dto.id_);
    while (statement.Step())
    {
      TransferJobDTOGet r(statement.ColumnString(0), statement.ColumnInt64(1), statement.ColumnString(2), statement.ColumnString(3));

      existings.push_back(r);
    }
    LOG(INFO) << "SaolaDatabase::SaveTransferJob END sql=" << "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=? LIMIT 1";
  }
  if (existings.empty())
  {
    LOG(INFO) << "SaolaDatabase::SaveTransferJob BEGIN sql=" << "INSERT INTO TransferJobs (id, queue_id, last_updated_time, creation_time) VALUES(?, ?, ?, ?)";
    Orthanc::SQLite::Statement statement(db_, "INSERT INTO TransferJobs (id, queue_id, last_updated_time, creation_time) VALUES(?, ?, ?, ?)");
    statement.BindString(0, dto.id_);
    statement.BindInt64(1, dto.queue_id_);
    statement.BindString(2, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.BindString(3, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.Run();
    LOG(INFO) << "SaolaDatabase::SaveTransferJob END sql=" << "INSERT INTO TransferJobs (id, queue_id, last_updated_time, creation_time) VALUES(?, ?, ?, ?)";


    result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
    result.creation_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
  }
  else
  {
    std::string sql = "UPDATE TransferJobs SET queue_id=" + std::to_string(dto.queue_id_) + " , last_updated_time=" + boost::posix_time::to_iso_string(Saola::GetNow()) + " WHERE id=" + dto.id_;
    LOG(INFO) << "SaolaDatabase::SaveTransferJob BEGIN sql=" << sql;
    Orthanc::SQLite::Statement statement(db_, "UPDATE TransferJobs SET queue_id=?, last_updated_time=? WHERE id=?");
    statement.BindInt64(0,  dto.queue_id_);
    statement.BindString(1, boost::posix_time::to_iso_string(Saola::GetNow()));
    statement.BindString(2, dto.id_);
    statement.Run();
    LOG(INFO) << "SaolaDatabase::SaveTransferJob END sql=" << sql;

    result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
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

bool SaolaDatabase::ResetFailedJob(const std::list<std::string> &ids)
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
  LOG(INFO) << "SaolaDatabase::ResetFailedJob sql=" << sql;
  Orthanc::SQLite::Statement statement(db_, sql);
  bool ok = statement.Run();

  transaction.Commit();
  return ok;
}

// bool SaolaDatabase::DeleteTransferJobByIds(const std::list<std::string> &ids)
// {
//   boost::mutex::scoped_lock lock(mutex_);

//   Orthanc::SQLite::Transaction transaction(db_);
//   transaction.Begin();
//   bool ok = true;
//   {
//     std::string sql = "DELETE FROM TransferJobs WHERE id IN (\"" + boost::algorithm::join(ids, "\",\"") + "\")";
//     LOG(INFO) << "[DeleteTransferJobByIds][SQL] sql=" << sql;

//     Orthanc::SQLite::Statement statement(db_, sql);
//     ok = statement.Run();
//   }

//   transaction.Commit();
//   return ok;
// }

bool SaolaDatabase::DeleteTransferJobByIds(const std::list<std::string> &ids)
{
  boost::mutex::scoped_lock lock(mutex_);

  if (ids.empty())
  {
    return true; // Nothing to delete
  }

  Orthanc::SQLite::Transaction transaction(db_);
  transaction.Begin();
  bool ok = true;
  
  // Create SQL with the right number of placeholders for the IN clause
  std::string sql = "DELETE FROM TransferJobs WHERE id IN (";
  
  // Add the appropriate number of parameter placeholders
  for (size_t i = 0; i < ids.size(); i++)
  {
    sql += (i > 0) ? ",?" : "?";
  }
  sql += ")";
  
  LOG(INFO) << "SaolaDatabase::DeleteTransferJobByIds sql=" << sql;
  
  // Prepare the statement
  Orthanc::SQLite::Statement statement(db_, sql);
  
  // Bind each ID parameter - using 0-based indexing as per your implementation
  int paramIndex = 0;
  for (const auto &id : ids)
  {
    statement.BindString(paramIndex++, id);
  }
  
  // Execute the statement
  ok = statement.Run();

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

bool SaolaDatabase::GetById(const std::string &id, TransferJobDTOGet &result)
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

bool SaolaDatabase::GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet> &results)
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

SaolaDatabase &SaolaDatabase::Instance()
{
  static SaolaDatabase instance;
  return instance;
}
