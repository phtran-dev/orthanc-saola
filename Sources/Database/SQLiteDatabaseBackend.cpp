#include "SQLiteDatabaseBackend.h"
#include "../Config/SaolaConfiguration.h"
#include "../TimeUtil.h"

#include <Logging.h>
#include <EmbeddedResources.h>
#include <SQLite/Transaction.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <SystemToolbox.h>

namespace Saola
{

  static const std::string JDBC_PREFIX = "jdbc:sqlite:";

  void SQLiteDatabaseBackend::Initialize()
  {
    // Mutex not rigidly needed here as Initialize is called from Open/OpenInMemory which should be locked or single-threaded
    // But for safety if called internally:
    // boost::mutex::scoped_lock lock(mutex_); // Open calls Initialize, and Open should be locked? 
    // Actually IDatabaseBackend::Open isn't locked by SaolaDatabase now. 
    // Initialize is private, called by Open/OpenInMemory.
    // So we lock in Open/OpenInMemory.

    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    if (!db_.DoesTableExist("StableEventQueues"))
    {
      std::string sql;
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_DATABASE);
      db_.Execute(sql);
    }

    transaction.Commit();

    // Performance tuning of SQLite with PRAGMAs
    // http://www.sqlite.org/pragma.html
    db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
    db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
    db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
    db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
  }

  void SQLiteDatabaseBackend::Open(const std::string& path)
  {
    boost::mutex::scoped_lock lock(mutex_);
    
    std::string dbPath = path;
    
    if (boost::starts_with(dbPath, JDBC_PREFIX))
    {
       dbPath = dbPath.substr(JDBC_PREFIX.length());
    }
    
    // Ensure directory exists
    boost::filesystem::path p(dbPath);
    if (p.has_parent_path())
    {
       Orthanc::SystemToolbox::MakeDirectory(p.parent_path().string());
    }

    db_.Open(dbPath);
    Initialize();
  }

  void SQLiteDatabaseBackend::OpenInMemory()
  {
    boost::mutex::scoped_lock lock(mutex_);
    db_.OpenInMemory();
    Initialize();
  }

  int64_t SQLiteDatabaseBackend::AddEvent(const StableEventDTOCreate& obj)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();
    {
      Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                           "INSERT INTO StableEventQueues (patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, last_updated_time, creation_time, next_scheduled_time) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
      statement.BindString(0, obj.patient_birth_date_.c_str());
      statement.BindString(1, obj.patient_id_.c_str());
      statement.BindString(2, obj.patient_name_.c_str());
      statement.BindString(3, obj.patient_sex_.c_str());
      statement.BindString(4, obj.accession_number_.c_str());
      statement.BindString(5, obj.iuid_.c_str());
      statement.BindString(6, obj.resource_id_.c_str());
      statement.BindString(7, obj.resouce_type_.c_str());
      statement.BindString(8, obj.app_id_.c_str());
      statement.BindString(9, obj.app_type_.c_str());
      statement.BindInt(10, obj.delay_);
      statement.BindString(11, boost::posix_time::to_iso_extended_string(Saola::GetNow()));
      statement.BindString(12, boost::posix_time::to_iso_extended_string(Saola::GetNow()));
      statement.BindString(13, Saola::GetNextXSecondsFromNowInString(obj.delay_));
      statement.Run();
    }

    transaction.Commit();
    return db_.GetLastInsertRowId();
  }

  bool SQLiteDatabaseBackend::DeleteEventByIds(const std::list<int64_t>& ids)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();
    bool ok = true;

    if (ids.empty())
    {
      // Delete all rows when no ids are specified
      {
        Orthanc::SQLite::Statement statement(db_, "DELETE FROM TransferJobs");
        ok &= statement.Run();
      }
      {
        Orthanc::SQLite::Statement statement(db_, "DELETE FROM StableEventQueues");
        ok &= statement.Run();
      }
      
    }
    else
    {
      {
        // Create SQL with the right number of placeholders for the IN clause
        std::string sql = "DELETE FROM TransferJobs WHERE queue_id IN (";
        
        // Add the appropriate number of parameter placeholders
        for (size_t i = 0; i < ids.size(); i++)
        {
          sql += (i > 0) ? ",?" : "?";
        }
        sql += ")";
        
        // Prepare the statement
        Orthanc::SQLite::Statement statement(db_, sql);
        
        // Bind each ID parameter - using 0-based indexing as per your implementation
        int paramIndex = 0;
        for (const auto &id : ids)
        {
          statement.BindInt64(paramIndex++, id);
        }
        
        // Execute the statement
        ok &= statement.Run();
      }

      {
        // Create SQL with the right number of placeholders for the IN clause
        std::string sql = "DELETE FROM StableEventQueues WHERE id IN (";
        
        // Add the appropriate number of parameter placeholders
        for (size_t i = 0; i < ids.size(); i++)
        {
          sql += (i > 0) ? ",?" : "?";
        }
        sql += ")";

        // Prepare the statement
        Orthanc::SQLite::Statement statement(db_, sql);
        
        // Bind each ID parameter
        int paramIndex = 0;
        for (const auto &id : ids)
        {
          statement.BindInt64(paramIndex++, id);
        }
        
        // Execute the statement
        ok &= statement.Run();
      }
    }

    transaction.Commit();
    return ok;
  }

  bool SQLiteDatabaseBackend::UpdateEvent(const StableEventDTOUpdate& obj)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();
    {
      Orthanc::SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                           "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=?, status=?, next_scheduled_time=? WHERE id=?");
      statement.BindString(0, obj.failed_reason_);
      statement.BindInt(1, obj.retry_);
      statement.BindString(2, obj.last_updated_time_);
      statement.BindString(3, obj.status_);
      statement.BindString(4, obj.next_scheduled_time_);
      statement.BindInt64(5, obj.id_);
      statement.Run();
    }

    transaction.Commit();
    return true;
  }

  bool SQLiteDatabaseBackend::ResetEvents(const std::list<int64_t>& ids)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    if (ids.empty())
    {
      // Reset all events when no ids are specified
      std::string sql = "UPDATE StableEventQueues SET status='PENDING', owner_id=NULL, failed_reason='Reset', retry=0, last_updated_time=?, next_scheduled_time=?, expiration_time=NULL";
      Orthanc::SQLite::Statement statement(db_, sql);
      statement.BindString(0, Saola::GetNextXSecondsFromNowInString(0));
      statement.BindString(1, Saola::GetNextXSecondsFromNowInString(0));
      statement.Run();
    }
    else
    {
      // Create SQL with placeholders for both update values and the IN clause
      std::string sql = "UPDATE StableEventQueues SET status='PENDING', owner_id=NULL, failed_reason='Reset', retry=0, last_updated_time=?, next_scheduled_time=?, expiration_time=NULL WHERE id IN (";
      
      // Add the appropriate number of parameter placeholders for IDs
      for (size_t i = 0; i < ids.size(); i++)
      {
        sql += (i > 0) ? ",?" : "?";
      }
      sql += ")";
      
      // Prepare the statement
      Orthanc::SQLite::Statement statement(db_, sql);
      
      // Bind the update field parameters first
      int paramIndex = 0;
      statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(0));
      statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(0));
      
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

  bool SQLiteDatabaseBackend::GetById(int64_t id, StableEventDTOGet& result)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    Orthanc::SQLite::Statement statement(db_, "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time FROM StableEventQueues WHERE id=?");
    statement.BindInt(0, id);
    bool ok = false;
    while (statement.Step())
    {
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);
      
      ok = true;
    }

    transaction.Commit();
    return ok;
  }

  bool SQLiteDatabaseBackend::GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    if (ids.empty()) {
      return false;
    }

    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();
    
    std::string query = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, "
                        "delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time "
                        "FROM StableEventQueues WHERE id IN (";
    
    std::string placeholders;
    for (size_t i = 0; i < ids.size(); i++) {
      if (i > 0) placeholders += ",";
      placeholders += "?";
    }
    query += placeholders + ")";
    
    Orthanc::SQLite::Statement statement(db_, query);
    
    int paramIndex = 0;
    for (const auto &id : ids) {
      statement.BindInt64(paramIndex++, id);
    }
    
    bool ok = false;
    while (statement.Step())
    {
      StableEventDTOGet result;
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);

      results.push_back(result);
      ok = true;
    }

    transaction.Commit();
    return ok;
  }

  void SQLiteDatabaseBackend::FindAll(const Pagination& page, std::list<StableEventDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    std::set<std::string> validColumns = {"id", "iuid", "resource_id", "resource_type", 
                                          "app_id", "app_type", "delay_sec", "retry", 
                                          "failed_reason", "last_updated_time", "creation_time"};
    
    std::string sortBy = "id";
    if (validColumns.find(page.sort_by_) != validColumns.end()) {
      sortBy = page.sort_by_;
    }

    std::string sql = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, "
                      "delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time "
                      "FROM StableEventQueues ORDER BY " + sortBy + " LIMIT ? OFFSET ?";

    Orthanc::SQLite::Statement statement(db_, sql);
    
    statement.BindInt(0, page.limit_);
    statement.BindInt64(1, page.offset_);

    while (statement.Step())
    {
      StableEventDTOGet result;
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);

      results.push_back(result);
    }

    transaction.Commit();
  }

  void SQLiteDatabaseBackend::FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    Orthanc::SQLite::Statement statement(db_, "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time FROM StableEventQueues WHERE retry <= ? ORDER BY retry ASC");

    statement.BindInt(0, retry);

    while (statement.Step())
    {
      StableEventDTOGet result;
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);

      results.push_back(result);
    }

    transaction.Commit();
  }

  void SQLiteDatabaseBackend::FindByAppTypeInRetryLessThan(const std::list<std::string>& appTypes, bool included, int retry, int limit, std::list<StableEventDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    if (appTypes.empty()) {
      return;
    }

    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    std::string baseQuery = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, "
                            "delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time "
                            "FROM StableEventQueues WHERE app_type ";

    std::string inOperator = included ? "IN" : "NOT IN";
    std::string inClause = inOperator + " (";
    
    for (size_t i = 0; i < appTypes.size(); i++) {
      inClause += (i > 0) ? ",?" : "?";
    }
    inClause += ")";
    
    std::string sql = baseQuery + inClause + " AND retry <= ? ORDER BY retry ASC LIMIT ?";
    
    Orthanc::SQLite::Statement statement(db_, sql);
    
    int paramIndex = 0;
    for (const auto& appType : appTypes) {
      statement.BindString(paramIndex++, appType);
    }
    
    statement.BindInt(paramIndex++, retry);  
    statement.BindInt(paramIndex, limit);
    
    while (statement.Step())
    {
      StableEventDTOGet result;
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);

      results.push_back(result);
    }

    transaction.Commit();
  }

  void SQLiteDatabaseBackend::Dequeue(const std::list<std::string>& appTypes, bool included, int retry, int limit, const std::string& owner, std::list<StableEventDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    if (appTypes.empty()) {
      return;
    }

    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    std::string inClause = std::string(included ? "IN" : "NOT IN") + " (";
    for (size_t i = 0; i < appTypes.size(); i++) {
      inClause += (i > 0) ? ",?" : "?";
    }
    inClause += ")";

    std::string expirationCase = "CASE app_type ";
    const auto& durations = SaolaConfiguration::Instance().GetJobLockDurations();
    for (size_t i = 0; i < durations.size(); i++) {
        expirationCase += "WHEN ? THEN ? ";
    }
    expirationCase += "ELSE ? END";

    std::string sql = "UPDATE StableEventQueues "
                      " SET status='PROCESSING', owner_id=?, last_updated_time=?, expiration_time=" + expirationCase + " "
                      " WHERE id IN ("
                        " SELECT id FROM StableEventQueues "
                        " WHERE "
                        " ("
                          " (status='PENDING' AND (owner_id IS NULL OR owner_id=?) AND next_scheduled_time <= ?) "
                          " OR "
                          " (status='PROCESSING' AND expiration_time < ?) "
                        ")"
                        " AND app_type " + inClause + " "
                        " AND retry <= ? "
                        " ORDER BY retry ASC, creation_time ASC "
                        " LIMIT ?"
                      ") "
                      " RETURNING id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, "
                      " delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time";

    Orthanc::SQLite::Statement statement(db_, sql);
    int paramIndex = 0;
    statement.BindString(paramIndex++, owner); // owner_id
    statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(0)); // last_updated_time
    
    for (const auto& item : durations) {
       statement.BindString(paramIndex++, item.first); // app_type
       statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(item.second)); // expiration_time
    }
    statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(SaolaConfiguration::Instance().GetDefaultJobLockDuration())); // expiration_time

    statement.BindString(paramIndex++, owner); // owner_id
    statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(0)); // last_updated_time
    statement.BindString(paramIndex++, Saola::GetNextXSecondsFromNowInString(0)); // expiration_time

    for (const auto& appType : appTypes) {
      statement.BindString(paramIndex++, appType); // app_type
    }

    statement.BindInt(paramIndex++, retry); // retry
    statement.BindInt(paramIndex++, limit); // limit

    while (statement.Step())
    {
      StableEventDTOGet result;
      result.id_ = statement.ColumnInt64(0);
      result.status_ = statement.ColumnString(1);
      result.owner_id_ = statement.ColumnString(2);
      result.patient_birth_date_ = statement.ColumnString(3);
      result.patient_id_ = statement.ColumnString(4);
      result.patient_name_ = statement.ColumnString(5);
      result.patient_sex_ = statement.ColumnString(6);
      result.accession_number_ = statement.ColumnString(7);
      result.iuid_ = statement.ColumnString(8);
      result.resource_id_ = statement.ColumnString(9);
      result.resource_type_ = statement.ColumnString(10);
      result.app_id_ = statement.ColumnString(11);
      result.app_type_ = statement.ColumnString(12);
      result.delay_sec_ = statement.ColumnInt(13);
      result.retry_ = statement.ColumnInt(14);
      result.failed_reason_ = statement.ColumnString(15);
      result.next_scheduled_time_ = statement.ColumnString(16);
      result.expiration_time_ = statement.ColumnString(17);
      result.last_updated_time_ = statement.ColumnString(18);
      result.creation_time_ = statement.ColumnString(19);

      results.push_back(result);
    }

    if (!results.empty())
    {
      // Update entry to increase retry by 1.
      // This is to prevent infinite jobs running even if the job is running in other replicas (services in load balancing mode)
      Orthanc::SQLite::Statement updateRetry(db_, "UPDATE StableEventQueues SET retry=? WHERE id=?");

      for (const auto& result : results)
      {
        updateRetry.Reset();
        updateRetry.BindInt(0, result.retry_ + 1);
        updateRetry.BindInt64(1, result.id_);
        updateRetry.Run();
      }
    }

    transaction.Commit();
  }

  void SQLiteDatabaseBackend::SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result)
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
      statement.BindString(2, boost::posix_time::to_iso_string(Saola::GetNow()));
      statement.BindString(3, boost::posix_time::to_iso_string(Saola::GetNow()));
      statement.Run();

      result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
      result.creation_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
    }
    else
    {
      Orthanc::SQLite::Statement statement(db_, "UPDATE TransferJobs SET queue_id=?, last_updated_time=? WHERE id=?");
      statement.BindInt64(0,  dto.queue_id_);
      statement.BindString(1, boost::posix_time::to_iso_string(Saola::GetNow()));
      statement.BindString(2, dto.id_);
      statement.Run();

      result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
      result.creation_time_ = existings.front().creation_time_;
    }

    transaction.Commit();

    result.id_ = dto.id_;
  }

  bool SQLiteDatabaseBackend::ResetFailedJob(const std::list<std::string>& ids)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    std::string sql = "UPDATE FailedJobs SET retry = 0";

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

  bool SQLiteDatabaseBackend::DeleteTransferJobByIds(const std::list<std::string>& ids)
  {
    boost::mutex::scoped_lock lock(mutex_);
    if (ids.empty())
    {
      return true;
    }

    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();
    bool ok = true;
    
    std::string sql = "DELETE FROM TransferJobs WHERE id IN (";
    
    for (size_t i = 0; i < ids.size(); i++)
    {
      sql += (i > 0) ? ",?" : "?";
    }
    sql += ")";
    
    Orthanc::SQLite::Statement statement(db_, sql);
    
    int paramIndex = 0;
    for (const auto &id : ids)
    {
      statement.BindString(paramIndex++, id);
    }
    
    ok = statement.Run();

    transaction.Commit();
    return ok;
  }

  bool SQLiteDatabaseBackend::DeleteTransferJobsByQueueId(int64_t id)
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

  bool SQLiteDatabaseBackend::GetById(const std::string& id, TransferJobDTOGet& result)
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

  bool SQLiteDatabaseBackend::GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results)
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

  bool SQLiteDatabaseBackend::GetTransferJobsByByQueueIds(const std::list<int64_t>& ids, std::list<TransferJobDTOGet>& results)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Orthanc::SQLite::Transaction transaction(db_);
    transaction.Begin();

    std::string query = "SELECT id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE queue_id IN (";
    std::string placeholders;
    for (size_t i = 0; i < ids.size(); i++) {
      if (i > 0) placeholders += ",";
      placeholders += "?";
    }
    query += placeholders + ")";
    Orthanc::SQLite::Statement statement(db_, query);

    int paramIndex = 0;
    for (const auto &id : ids)
    {
      statement.BindInt64(paramIndex++, id);
    }

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
}
