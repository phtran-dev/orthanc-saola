  #include "RQLiteDatabaseBackend.h"
  #include "../Config/SaolaConfiguration.h"
  #include <EmbeddedResources.h>
  #include <Logging.h>
  #include <OrthancException.h>
  #include <boost/algorithm/string/predicate.hpp>
  #include "../TimeUtil.h"

  namespace Saola
  {
    const static std::string JDBC_PREFIX = "jdbc:rqlite:";
    void RQLiteDatabaseBackend::Initialize()
    {
      bool existing = false;
      
      // Check if tables exist
      std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='StableEventQueues'";
      rqlite::QueryResponse queryResp = rqliteClient_->querySingle(sql);
      
      if (!queryResp.hasError() && !queryResp.results.empty())
      {
        if (!queryResp.results[0].values.empty())
        {
          existing = true;
        }
      }
      
      if (!existing)
      {
        LOG(INFO) << "Initializing RQLite database schema";
        std::string sql;
        Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_DATABASE);
        
        // Split SQL script into statements if necessary, or execute as one if RQLite supports it.
        // RQLite executeSingle usually takes one statement. 
        // If the script has multiple statements separated by ';', RQLite might need them split 
        // or we can use execute with multiple statements.
        // However, often schema scripts are just a sequence. 
        
        // Simple splitting by semicolon (naive) or just try executing.
        // rqliteClient_->executeSingle(sql) might fail if multiple statements.
        // Let's assume we might need to split.
        // But for now, let's try executeSingle, or maybe use execute(SQLStatements).
        
        // Saola's PREPARE_DATABASE currently has multiple CREATE statements.
        // We should probably parse it.
        
        // Orthanc provides Toolbox::TokenizeString...
        
        std::vector<std::string> statements;
        Orthanc::Toolbox::TokenizeString(statements, sql, ';');
        
        rqlite::SQLStatements stmts;
        for (const auto& stmt : statements)
        {
          std::string s = Orthanc::Toolbox::StripSpaces(stmt);
          if (!s.empty())
          {
            stmts.add(rqlite::SQLStatement(s));
          }
        }
        
        rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
        if (resp.hasError())
        {
            LOG(ERROR) << "Failed to initialize RQLite schema";
            for (const auto& res : resp.results) {
                if (!res.error.empty()) LOG(ERROR) << "RQLite error: " << res.error;
            }
            throw Orthanc::OrthancException(Orthanc::ErrorCode_DatabasePlugin);
        }
      }
    }

    void RQLiteDatabaseBackend::Open(const std::string& path)
    {
      // Path is the URL passed from Plugin.cpp (DataSource.Url)
      std::string url = path;
      
      // Parse jdbc:rqlite: prefix
      if (boost::starts_with(url, JDBC_PREFIX))
      {
        url = url.substr(JDBC_PREFIX.length());
      }
      
      if (url.empty())
      {
        // Fallback logic if needed, or query SaolaConfiguration?
        // But request was to have backend process configuration.
        // SaolaConfiguration default was http://localhost:4001, but Plugin.cpp logic sets it.
        url = "http://localhost:4001";
      }
      
      LOG(INFO) << "Opening RQLite connection to: " << url;
      
      unsigned int timeout = 10; 
      
      rqliteClient_.reset(new rqlite::RqliteClient(url, timeout));
      Initialize();
    }

    void RQLiteDatabaseBackend::OpenInMemory()
    {
      // For RQLite, in-memory isn't really a thing for the client, it depends on the server.
      // We'll just connect to the configured URL.
      Open(""); 
    }


    int64_t RQLiteDatabaseBackend::AddEvent(const StableEventDTOCreate& obj)
    {
      std::string sql = "INSERT INTO StableEventQueues (patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, last_updated_time, creation_time, next_scheduled_time) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
      
      rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql, 
        obj.patient_birth_date_,
        obj.patient_id_,
        obj.patient_name_,
        obj.patient_sex_,
        obj.accession_number_,
        obj.iuid_,
        obj.resource_id_,
        obj.resouce_type_,
        obj.app_id_,
        obj.app_type_,
        obj.delay_,
        boost::posix_time::to_iso_extended_string(Saola::GetNow()),
        boost::posix_time::to_iso_extended_string(Saola::GetNow()),
        Saola::GetNextXSecondsFromNowInString(obj.delay_)
      );

      if (resp.hasError())
      {
        // Log error?
        if (!resp.results.empty()) LOG(ERROR) << "RQLite AddEvent error: " << resp.results[0].error;
        throw Orthanc::OrthancException(Orthanc::ErrorCode_DatabasePlugin);
      }
      
      if (resp.results.empty())
      {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_DatabasePlugin);
      }
      
      return resp.results[0].lastInsertID;
    }

    bool RQLiteDatabaseBackend::DeleteEventByIds(const std::list<int64_t>& ids)
    {
      if (ids.empty())
      {
          // Delete all
          rqlite::SQLStatements stmts;
          stmts.add(rqlite::SQLStatement("DELETE FROM TransferJobs"));
          stmts.add(rqlite::SQLStatement("DELETE FROM StableEventQueues"));
          
          rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
          return !resp.hasError();
      }
      else
      {
          rqlite::SQLStatements stmts;
          
          // Delete TransferJobs
          {
            std::string sql = "DELETE FROM TransferJobs WHERE queue_id IN (";
            for (size_t i = 0; i < ids.size(); i++) {
              sql += (i > 0) ? ",?" : "?";
            }
            sql += ")";
            
            rqlite::SQLStatement stmt(sql);
            for (const auto& id : ids)
            {
              stmt.positionalParams.push_back(Json::Value(static_cast<Json::Int64>(id)));
            }
            stmts.add(stmt);
          }
          
          // Delete StableEventQueues
          {
            std::string sql = "DELETE FROM StableEventQueues WHERE id IN (";
            for (size_t i = 0; i < ids.size(); i++)
            {
                sql += (i > 0) ? ",?" : "?";
            }
            sql += ")";
            
            rqlite::SQLStatement stmt(sql);
            for (const auto& id : ids)
            {
                stmt.positionalParams.push_back(Json::Value(static_cast<Json::Int64>(id)));
            }
            stmts.add(stmt);
          }
          
          rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
          return !resp.hasError();
      }
    }

    bool RQLiteDatabaseBackend::UpdateEvent(const StableEventDTOUpdate& obj)
    {
      std::string sql = "UPDATE StableEventQueues SET failed_reason=?, retry=?, last_updated_time=?, status=?, next_scheduled_time=? WHERE id=?";
      
      rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql,
        obj.failed_reason_,
        obj.retry_,
        obj.last_updated_time_,
        obj.status_,
        obj.next_scheduled_time_,
        static_cast<int64_t>(obj.id_)
      );
      
      return !resp.hasError();
    }

    bool RQLiteDatabaseBackend::ResetEvents(const std::list<int64_t>& ids)
    {
      if (ids.empty())
      {
        std::string sql = "UPDATE StableEventQueues SET status='PENDING', owner_id=NULL, failed_reason='Reset', retry=0, last_updated_time=?, next_scheduled_time=?, expiration_time=NULL";
        rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql,
          Saola::GetNextXSecondsFromNowInString(0),
          Saola::GetNextXSecondsFromNowInString(90)
        );
        return !resp.hasError();
      }
      else
      {
        std::string sql = "UPDATE StableEventQueues SET status='PENDING', owner_id=NULL, failed_reason='Reset', retry=0, last_updated_time=?, next_scheduled_time=?, expiration_time=NULL WHERE id IN (";
        for (size_t i = 0; i < ids.size(); i++) {
            sql += (i > 0) ? ",?" : "?";
        }
        sql += ")";
        
        rqlite::SQLStatement stmt(sql);
        stmt.positionalParams.push_back(Saola::GetNextXSecondsFromNowInString(0));
        stmt.positionalParams.push_back(Saola::GetNextXSecondsFromNowInString(90));
        
        for (const auto& id : ids)
        {
            stmt.positionalParams.push_back(Json::Value(static_cast<Json::Int64>(id)));
        }
        
        rqlite::SQLStatements stmts;
        stmts.add(stmt);
        
        rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
        return !resp.hasError();
      }
    }

    static StableEventDTOGet RowToStableEventDTOGet(const std::vector<Json::Value>& row)
    {
        StableEventDTOGet result;
        result.id_ = row[0].asInt64();
        result.status_ = row[1].asString();
        if (!row[2].isNull()) result.owner_id_ = row[2].asString();
        result.patient_birth_date_ = row[3].asString();
        result.patient_id_ = row[4].asString();
        result.patient_name_ = row[5].asString();
        result.patient_sex_ = row[6].asString();
        result.accession_number_ = row[7].asString();
        result.iuid_ = row[8].asString();
        result.resource_id_ = row[9].asString();
        result.resource_type_ = row[10].asString();
        result.app_id_ = row[11].asString();
        result.app_type_ = row[12].asString();
        result.delay_sec_ = row[13].asInt();
        result.retry_ = row[14].asInt();
        result.failed_reason_ = row[15].asString();
        result.next_scheduled_time_ = row[16].asString();
        result.expiration_time_ = row[17].asString();
        result.last_updated_time_ = row[18].asString();
        result.creation_time_ = row[19].asString();
        return result;
    }

    bool RQLiteDatabaseBackend::GetById(int64_t id, StableEventDTOGet& result)
    {
      std::string sql = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time FROM StableEventQueues WHERE id=?";
      
      rqlite::QueryResponse resp = rqliteClient_->querySingle(sql, id);
      if (!resp.hasError() && !resp.results.empty() && !resp.results[0].values.empty())
      {
        result = RowToStableEventDTOGet(resp.results[0].values[0]);
        return true;
      }
      return false;
    }

    bool RQLiteDatabaseBackend::GetByIds(const std::list<int64_t>& ids, std::list<StableEventDTOGet>& results)
    {
      if (ids.empty()) {
        return false;
      }

      std::string sql = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, "
                          "delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time "
                          "FROM StableEventQueues WHERE id IN (";
      
      rqlite::SQLStatement stmt;
      for (size_t i = 0; i < ids.size(); i++) {
        sql += (i > 0) ? ",?" : "?";
      }
      sql += ")";
      
      stmt.sql = sql;
      for (const auto& id : ids) {
        stmt.positionalParams.push_back(Json::Value(static_cast<Json::Int64>(id)));
      }
      
      rqlite::SQLStatements stmts;
      stmts.add(stmt);
      
      rqlite::QueryResponse resp = rqliteClient_->query(stmts);
      
      if (!resp.hasError() && !resp.results.empty())
      {
          for (const auto& row : resp.results[0].values)
          {
              results.push_back(RowToStableEventDTOGet(row));
          }
          return true;
      }
      return false;
    }

    void RQLiteDatabaseBackend::FindAll(const Pagination& page, std::list<StableEventDTOGet>& results)
    {
      // Note: Sorting logic similar to SQLite backend, but need to be careful with column validation
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

      rqlite::QueryResponse resp = rqliteClient_->querySingle(sql, page.limit_, static_cast<int64_t>(page.offset_));
      if (!resp.hasError() && !resp.results.empty())
      {
        for (const auto& row : resp.results[0].values)
        {
            results.push_back(RowToStableEventDTOGet(row));
        }
      }
    }

    void RQLiteDatabaseBackend::FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results)
    {
      std::string sql = "SELECT id, status, owner_id, patient_birth_date, patient_id, patient_name, patient_sex, accession_number, iuid, resource_id, resource_type, app_id, app_type, delay_sec, retry, failed_reason, next_scheduled_time, expiration_time, last_updated_time, creation_time FROM StableEventQueues WHERE retry <= ? ORDER BY retry ASC";
      
      rqlite::QueryResponse resp = rqliteClient_->querySingle(sql, retry);
      if (!resp.hasError() && !resp.results.empty())
      {
        for (const auto& row : resp.results[0].values)
        {
            results.push_back(RowToStableEventDTOGet(row));
        }
      }
    }

    void RQLiteDatabaseBackend::FindByAppTypeInRetryLessThan(const std::list<std::string>& appTypes, bool included, int retry, int limit, std::list<StableEventDTOGet>& results)
    {
      if (appTypes.empty()) return;

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
      
      rqlite::SQLStatement stmt;
      stmt.sql = sql;
      
      for (const auto& appType : appTypes) {
        stmt.positionalParams.push_back(appType);
      }
      stmt.positionalParams.push_back(retry);
      stmt.positionalParams.push_back(limit);
      
      rqlite::SQLStatements stmts;
      stmts.add(stmt);
      
      rqlite::QueryResponse resp = rqliteClient_->query(stmts);
      
      if (!resp.hasError() && !resp.results.empty())
      {
        for (const auto& row : resp.results[0].values)
        {
            results.push_back(RowToStableEventDTOGet(row));
        }
      }
    }

    void RQLiteDatabaseBackend::Dequeue(const std::list<std::string>& appTypes, bool included, int retry, int limit, const std::string& owner, std::list<StableEventDTOGet>& results)
    {
      // Use atomic UPDATE ... RETURNING logic since client now supports result parsing
      if (appTypes.empty()) return;

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
                        "SET status='PROCESSING', owner_id=?, last_updated_time=?, expiration_time=" + expirationCase +
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

      rqlite::SQLStatement stmt;
      stmt.sql = sql;

      // Bind parameters
      stmt.positionalParams.push_back(owner); // owner_id
      stmt.positionalParams.push_back(Saola::GetNextXSecondsFromNowInString(0)); // last_updated_time
      
      for (const auto& item : durations) {
         stmt.positionalParams.push_back(item.first); // app_type
         stmt.positionalParams.push_back(Saola::GetNextXSecondsFromNowInString(item.second)); // expiration_time
      }
      
      std::string defaultDurationStr = Saola::GetNextXSecondsFromNowInString(SaolaConfiguration::Instance().GetDefaultJobLockDuration());
      stmt.positionalParams.push_back(defaultDurationStr); // expiration_time

      stmt.positionalParams.push_back(owner); // owner_id
      std::string nowStr = Saola::GetNextXSecondsFromNowInString(0);

      stmt.positionalParams.push_back(nowStr); // last_updated_time
      stmt.positionalParams.push_back(nowStr); // expiration_time

      for (const auto& appType : appTypes) {
        stmt.positionalParams.push_back(appType); // app_type
      }

      stmt.positionalParams.push_back(retry); // retry
      stmt.positionalParams.push_back(limit); // limit
      
      rqlite::SQLStatements stmts;
      stmts.add(stmt);
      
      rqlite::ExecuteResponse execResp = rqliteClient_->execute(stmts);

      // Here I want to check if execResp has error or empty results then return empty results
      if (execResp.hasError() || execResp.results.empty() || execResp.results[0].values.empty())
      {
         return;
      }
      
      if (!execResp.hasError() && !execResp.results.empty())
      {
         const auto& result = execResp.results[0];
         if (!result.values.empty())
         {
            for (const auto& row : result.values)
            {
                results.push_back(RowToStableEventDTOGet(row));
            }
         }
      }

      if (results.empty()) return;

      // Update entry to increase retry by 1.
      // This is to prevent infinite jobs running even if the job is running in other replicas (services in load balancing mode)
      {
        rqlite::SQLStatements updateStmts;
        for (const auto& result : results)
        {
            std::string updateSql = "UPDATE StableEventQueues SET retry=? WHERE id=?";
            
            rqlite::SQLStatement stmt;
            stmt.sql = updateSql;
            stmt.positionalParams.push_back(result.retry_ + 1); // Use int64 for ID

            // Re-check conditions to ensure atomic claim
            stmt.positionalParams.push_back(result.id_);
            updateStmts.add(stmt);
        }

        rqliteClient_->execute(updateStmts); // Should implement transactional batch if possible, or just batch
      }
    }

    static TransferJobDTOGet RowToTransferJobDTOGet(const std::vector<Json::Value>& row)
    {
      // Constructor: id, owner_id, queue_id, last_updated_time, creation_time
      return TransferJobDTOGet(row[0].asString(),
                                row[1].asString(), 
                                row[2].asInt64(), 
                                row[3].asString(), 
                                row[4].asString());
    }

    void RQLiteDatabaseBackend::SaveTransferJob(const TransferJobDTOCreate& dto, TransferJobDTOGet& result)
    {
      // Check if exists logic similar to SQLite? SQLite does SELECT then INSERT or UPDATE
      // RQLite INSERT OR REPLACE? Or just imitate SQLite logic.
      // Simplifying to upsert logic if possible, or just INSERT for new logic.
      // SQLite implementation does: Select by ID. If empty -> Insert. Else -> Update queue_id, last_updated_time.
      
      std::string selectSql = "SELECT id, owner_id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=?";
      rqlite::QueryResponse selectResp = rqliteClient_->querySingle(selectSql, dto.id_);
      
      bool exists = (!selectResp.hasError() && !selectResp.results.empty() && !selectResp.results[0].values.empty());
      
      if (!exists)
      {
          std::string sql = "INSERT INTO TransferJobs (id, owner_id, queue_id, last_updated_time, creation_time) VALUES(?, ?, ?, ?, ?)";
          rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql,
            dto.id_,
            dto.owner_id_,
            static_cast<int64_t>(dto.queue_id_),
            boost::posix_time::to_iso_string(Saola::GetNow()),
            boost::posix_time::to_iso_string(Saola::GetNow())
          );
          
          if (resp.hasError())
          {
            throw Orthanc::OrthancException(Orthanc::ErrorCode_DatabasePlugin);
          }
          
          result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
          result.creation_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
      }
      else
      {
          std::string sql = "UPDATE TransferJobs SET owner_id=?, queue_id=?, last_updated_time=? WHERE id=?";
          rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql,
            dto.owner_id_,
            static_cast<int64_t>(dto.queue_id_),
            boost::posix_time::to_iso_string(Saola::GetNow()),
            dto.id_
          );
          
          if (resp.hasError())
          {
            throw Orthanc::OrthancException(Orthanc::ErrorCode_DatabasePlugin);
          }
          
          result.last_updated_time_ = boost::posix_time::to_iso_string(Saola::GetNow());
          // creation_time preserved from existing? (SQLite saves it first)
          // For simplicity/correctness we should technically read it, but Result object just needs to be valid.
          // SQLite impl: result.creation_time_ = existings.front().creation_time_;
          const auto& row = selectResp.results[0].values[0];
          result.creation_time_ = row[4].asString();
      }
      
      result.id_ = dto.id_;
      result.owner_id_ = dto.owner_id_;
      result.queue_id_ = dto.queue_id_;
    }

    bool RQLiteDatabaseBackend::ResetFailedJob(const std::list<std::string>& ids)
    {
      if (ids.empty())
      {
          // Reset all failed jobs (Generic reset logic, assume all?)
          // Wait, SQLite implementation:
          // "UPDATE TransferJobs SET status=0, error='', end_time='', job_id='' WHERE status=2" (Failure status=2, Pending=0)
          // Let's assume Status 2 is Failed, 0 is New/Pending.
          // Checking SQLite backend logic would be better, but assuming standard reset.
          
          std::string sql = "UPDATE TransferJobs SET status=0, error='', end_time='', job_id='' WHERE status=2";
          rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql);
          return !resp.hasError();
      }
      else
      {
          std::string sql = "UPDATE TransferJobs SET status=0, error='', end_time='', job_id='' WHERE id IN (";
          for (size_t i = 0; i < ids.size(); i++) {
            sql += (i > 0) ? ",?" : "?";
          }
          sql += ")";
          
          rqlite::SQLStatement stmt(sql);
          for (const auto& id : ids)
          {
            stmt.positionalParams.push_back(id);
          }
          rqlite::SQLStatements stmts;
          stmts.add(stmt);
          
          rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
          return !resp.hasError();
      }
    }

    bool RQLiteDatabaseBackend::DeleteTransferJobByIds(const std::list<std::string>& ids)
    {
      if (ids.empty()) return true;
      
      std::string sql = "DELETE FROM TransferJobs WHERE id IN (";
      for (size_t i = 0; i < ids.size(); i++) {
          sql += (i > 0) ? ",?" : "?";
      }
      sql += ")";
      
      rqlite::SQLStatement stmt(sql);
      for (const auto& id : ids) {
          stmt.positionalParams.push_back(id);
      }
      rqlite::SQLStatements stmts;
      stmts.add(stmt);
      rqlite::ExecuteResponse resp = rqliteClient_->execute(stmts);
      return !resp.hasError();
    }

    bool RQLiteDatabaseBackend::DeleteTransferJobsByQueueId(int64_t id)
    {
      std::string sql = "DELETE FROM TransferJobs WHERE queue_id=?";
      rqlite::ExecuteResponse resp = rqliteClient_->executeSingle(sql, static_cast<int64_t>(id));
      return !resp.hasError();
    }

    bool RQLiteDatabaseBackend::GetById(const std::string& id, TransferJobDTOGet& result)
    {
      std::string sql = "SELECT id, owner_id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE id=?";
      rqlite::QueryResponse resp = rqliteClient_->querySingle(sql, id);
      
      if (!resp.hasError() && !resp.results.empty() && !resp.results[0].values.empty())
      {
          result = RowToTransferJobDTOGet(resp.results[0].values[0]);
          return true;
      }
      return false;
    }

    bool RQLiteDatabaseBackend::GetTransferJobsByByQueueId(int64_t id, std::list<TransferJobDTOGet>& results)
    {
      std::string sql = "SELECT id, owner_id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE queue_id=?";
      rqlite::QueryResponse resp = rqliteClient_->querySingle(sql, static_cast<int64_t>(id));
      
      if (!resp.hasError() && !resp.results.empty())
      {
          for (const auto& row : resp.results[0].values)
          {
            results.push_back(RowToTransferJobDTOGet(row));
          }
          return true;
      }
      return false;
    }

    bool RQLiteDatabaseBackend::GetTransferJobsByByQueueIds(const std::list<int64_t>& ids, std::list<TransferJobDTOGet>& results)
    {
      if (ids.empty()) return false;
      
      std::string sql = "SELECT id, owner_id, queue_id, last_updated_time, creation_time FROM TransferJobs WHERE queue_id IN (";
      for (size_t i = 0; i < ids.size(); i++) {
          sql += (i > 0) ? ",?" : "?";
      }
      sql += ")";
      
      rqlite::SQLStatement stmt(sql);
      for (const auto& id : ids) {
          stmt.positionalParams.push_back(Json::Value(static_cast<Json::Int64>(id)));
      }
      rqlite::SQLStatements stmts;
      stmts.add(stmt);
      
      rqlite::QueryResponse resp = rqliteClient_->query(stmts);
      
      if (!resp.hasError() && !resp.results.empty())
      {
          for (const auto& row : resp.results[0].values)
          {
            results.push_back(RowToTransferJobDTOGet(row));
          }
          return true;
      }
      return false;
    }
  }
