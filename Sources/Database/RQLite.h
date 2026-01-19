#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <iostream>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <json/value.h>

#include <Toolbox.h>
#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace rqlite
{

  // Enum for read consistency level
  enum class ReadConsistencyLevel
  {
    UNKNOWN,
    NONE,
    WEAK,
    STRONG,
    LINEARIZABLE,
    AUTO
  };

  // Convert ReadConsistencyLevel to string
  std::string toString(ReadConsistencyLevel level);

  // SQLStatement class for handling SQL queries and parameters
  class SQLStatement
  {
  public:
    std::string sql;
    std::vector<Json::Value> positionalParams;
    std::map<std::string, Json::Value> namedParams;

    SQLStatement() {}
    SQLStatement(const std::string &sql) : sql(sql) {}

    // Create a new SQL statement with positional parameters
    template <typename... Args>
    static SQLStatement newSQLStatement(const std::string &stmt, Args... args)
    {
      SQLStatement s(stmt);
      s.addParams(args...);
      return s;
    }

    // Convert to JSON representation
    Json::Value toJson() const
    {
      if (!namedParams.empty())
      {
        Json::Value arr(Json::arrayValue);
        arr.append(sql);

        Json::Value paramsObj(Json::objectValue);
        for (const auto &pair : namedParams)
        {
          paramsObj[pair.first] = pair.second;
        }
        arr.append(paramsObj);

        return arr;
      }
      else if (!positionalParams.empty())
      {
        Json::Value arr(Json::arrayValue);
        arr.append(sql);

        for (const auto &param : positionalParams)
        {
          arr.append(param);
        }

        return arr;
      }
      else
      {
        return Json::Value(sql);
      }
    }

  private:
    // Base case for parameter recursion
    void addParams() {}

    // Add a single parameter
    template <typename T>
    void addParams(T param)
    {
      Json::Value value;
      setValue(value, param);
      positionalParams.push_back(value);
    }

    // Add multiple parameters recursively
    template <typename T, typename... Args>
    void addParams(T first, Args... rest)
    {
      Json::Value value;
      setValue(value, first);
      positionalParams.push_back(value);
      addParams(rest...);
    }

    // Helper to set JSON value from different C++ types
    template <typename T>
    void setValue(Json::Value &value, const T &param)
    {
      value = param; // Default JsonCpp conversion
    }
  };

  // Collection of SQL statements
  class SQLStatements : public std::vector<SQLStatement>
  {
  public:
    Json::Value toJson() const
    {
      Json::Value arr(Json::arrayValue);
      for (const auto &stmt : *this)
      {
        arr.append(stmt.toJson());
      }
      return arr;
    }

    void add(const SQLStatement &stmt)
    {
      this->push_back(stmt);
    }
  };

  // Options classes
  struct BackupOptions
  {
    std::string format;
    bool vacuum = false;
    bool compress = false;
    bool noLeader = false;
    bool redirect = false;

    std::string toQueryString() const
    {
      std::ostringstream query;
      bool first = true;

      if (!format.empty())
      {
        std::string encoded_format;
        Orthanc::Toolbox::UriEncode(encoded_format, format);

        query << (first ? "?" : "&") << "fmt=" << encoded_format;
        first = false;
      }
      if (vacuum)
      {
        query << (first ? "?" : "&") << "vacuum=true";
        first = false;
      }
      if (compress)
      {
        query << (first ? "?" : "&") << "compress=true";
        first = false;
      }
      if (noLeader)
      {
        query << (first ? "?" : "&") << "noleader=true";
        first = false;
      }
      if (redirect)
      {
        query << (first ? "?" : "&") << "redirect=true";
        first = false;
      }
      return query.str();
    }
  };

  struct LoadOptions
  {
    bool redirect = false;

    std::string toQueryString() const
    {
      if (redirect)
      {
        return "?redirect=true";
      }
      return "";
    }
  };

  struct ExecuteOptions
  {
    bool transaction = false;
    bool pretty = false;
    bool timings = false;
    bool queue = false;
    bool wait = false;
    std::chrono::milliseconds timeout{0};

    std::string toQueryString() const
    {
      std::ostringstream query;
      bool first = true;

      if (transaction)
      {
        query << (first ? "?" : "&") << "transaction=true";
        first = false;
      }
      if (pretty)
      {
        query << (first ? "?" : "&") << "pretty=true";
        first = false;
      }
      if (timings)
      {
        query << (first ? "?" : "&") << "timings=true";
        first = false;
      }
      if (queue)
      {
        query << (first ? "?" : "&") << "queue=true";
        first = false;
      }
      if (wait)
      {
        query << (first ? "?" : "&") << "wait=true";
        first = false;
      }
      if (timeout.count() > 0)
      {
        query << (first ? "?" : "&") << "timeout=" << timeout.count() << "ms";
        first = false;
      }
      return query.str();
    }
  };

  struct QueryOptions
  {
    std::chrono::milliseconds timeout{0};
    bool pretty = false;
    bool timings = false;
    bool associative = false;
    bool blobAsArray = false;
    ReadConsistencyLevel level = ReadConsistencyLevel::UNKNOWN;
    std::chrono::milliseconds linearizableTimeout{0};
    std::chrono::milliseconds freshness{0};
    bool freshnessStrict = false;

    std::string toQueryString() const
    {
      std::ostringstream query;
      bool first = true;

      if (timeout.count() > 0)
      {
        query << (first ? "?" : "&") << "timeout=" << timeout.count() << "ms";
        first = false;
      }
      if (pretty)
      {
        query << (first ? "?" : "&") << "pretty=true";
        first = false;
      }
      if (timings)
      {
        query << (first ? "?" : "&") << "timings=true";
        first = false;
      }
      if (associative)
      {
        query << (first ? "?" : "&") << "associative=true";
        first = false;
      }
      if (blobAsArray)
      {
        query << (first ? "?" : "&") << "blob_array=true";
        first = false;
      }
      if (level != ReadConsistencyLevel::UNKNOWN)
      {
        query << (first ? "?" : "&") << "level=" << toString(level);
        first = false;
      }
      if (linearizableTimeout.count() > 0)
      {
        query << (first ? "?" : "&") << "linearizable_timeout=" << linearizableTimeout.count() << "ms";
        first = false;
      }
      if (freshness.count() > 0)
      {
        query << (first ? "?" : "&") << "freshness=" << freshness.count() << "ms";
        first = false;
      }
      if (freshnessStrict)
      {
        query << (first ? "?" : "&") << "freshness_strict=true";
        first = false;
      }
      return query.str();
    }
  };

  struct RequestOptions
  {
    bool transaction = false;
    std::chrono::milliseconds timeout{0};
    bool pretty = false;
    bool timings = false;
    bool associative = false;
    bool blobAsArray = false;
    ReadConsistencyLevel level = ReadConsistencyLevel::UNKNOWN;
    std::chrono::milliseconds linearizableTimeout{0};
    std::chrono::milliseconds freshness{0};
    bool freshnessStrict = false;

    std::string toQueryString() const
    {
      std::ostringstream query;
      bool first = true;

      if (transaction)
      {
        query << (first ? "?" : "&") << "transaction=true";
        first = false;
      }
      if (timeout.count() > 0)
      {
        query << (first ? "?" : "&") << "timeout=" << timeout.count() << "ms";
        first = false;
      }
      if (pretty)
      {
        query << (first ? "?" : "&") << "pretty=true";
        first = false;
      }
      if (timings)
      {
        query << (first ? "?" : "&") << "timings=true";
        first = false;
      }
      if (associative)
      {
        query << (first ? "?" : "&") << "associative=true";
        first = false;
      }
      if (blobAsArray)
      {
        query << (first ? "?" : "&") << "blob_array=true";
        first = false;
      }
      if (level != ReadConsistencyLevel::UNKNOWN)
      {
        query << (first ? "?" : "&") << "level=" << toString(level);
        first = false;
      }
      if (linearizableTimeout.count() > 0)
      {
        query << (first ? "?" : "&") << "linearizable_timeout=" << linearizableTimeout.count() << "ms";
        first = false;
      }
      if (freshness.count() > 0)
      {
        query << (first ? "?" : "&") << "freshness=" << freshness.count() << "ms";
        first = false;
      }
      if (freshnessStrict)
      {
        query << (first ? "?" : "&") << "freshness_strict=true";
        first = false;
      }
      return query.str();
    }
  };

  struct NodeOptions
  {
    std::chrono::milliseconds timeout{0};
    bool pretty = false;
    bool nonVoters = false;
    std::string version;

    std::string toQueryString() const
    {
      std::ostringstream query;
      bool first = true;

      if (timeout.count() > 0)
      {
        query << (first ? "?" : "&") << "timeout=" << timeout.count() << "ms";
        first = false;
      }
      if (pretty)
      {
        query << (first ? "?" : "&") << "pretty=true";
        first = false;
      }
      if (nonVoters)
      {
        query << (first ? "?" : "&") << "non_voters=true";
        first = false;
      }
      if (!version.empty())
      {
        std::string encoded_version;
        Orthanc::Toolbox::UriEncode(encoded_version, version);

        query << (first ? "?" : "&") << "ver=" << encoded_version;
        first = false;
      }
      return query.str();
    }
  };

  // Response types
  struct ExecuteResult
  {
    int64_t lastInsertID = 0;
    int64_t rowsAffected = 0;
    double time = 0.0;
    std::string error;
    std::vector<std::string> columns;
    std::vector<std::string> types;
    std::vector<std::vector<Json::Value>> values;

    static ExecuteResult fromJson(const Json::Value &json)
    {
      ExecuteResult result;
      if (json.isMember("last_insert_id"))
        result.lastInsertID = json["last_insert_id"].asInt64();
      if (json.isMember("rows_affected"))
        result.rowsAffected = json["rows_affected"].asInt64();
      if (json.isMember("time"))
        result.time = json["time"].asDouble();
      if (json.isMember("error"))
        result.error = json["error"].asString();

      // Support for RETURNING clause response in execute
      if (json.isMember("columns") && json["columns"].isArray())
      {
        for (const auto &col : json["columns"])
        {
          result.columns.push_back(col.asString());
        }
      }

      if (json.isMember("types") && json["types"].isArray())
      {
        for (const auto &type : json["types"])
        {
          result.types.push_back(type.asString());
        }
      }

      if (json.isMember("values") && json["values"].isArray())
      {
        for (const auto &row : json["values"])
        {
          std::vector<Json::Value> rowValues;
          for (const auto &val : row)
          {
            rowValues.push_back(val);
          }
          result.values.push_back(rowValues);
        }
      }

      return result;
    }
  };

  struct ExecuteResponse
  {
    std::vector<ExecuteResult> results;
    double time = 0.0;
    int64_t sequenceNumber = 0;

    static ExecuteResponse fromJson(const Json::Value &json)
    {
      ExecuteResponse response;

      if (json.isMember("results") && json["results"].isArray())
      {
        for (const auto &resultJson : json["results"])
        {
          response.results.push_back(ExecuteResult::fromJson(resultJson));
        }
      }

      if (json.isMember("time"))
        response.time = json["time"].asDouble();

      if (json.isMember("sequence_number"))
        response.sequenceNumber = json["sequence_number"].asInt64();

      return response;
    }

    bool hasError() const
    {
      for (const auto &result : results)
      {
        if (!result.error.empty())
        {
          return true;
        }
      }
      return false;
    }
  };

  struct QueryResult
  {
    std::vector<std::string> columns;
    std::vector<std::string> types;
    std::vector<std::vector<Json::Value>> values;
    double time = 0.0;
    std::string error;

    static QueryResult fromJson(const Json::Value &json)
    {
      QueryResult result;

      if (json.isMember("columns") && json["columns"].isArray())
      {
        for (const auto &col : json["columns"])
        {
          result.columns.push_back(col.asString());
        }
      }

      if (json.isMember("types") && json["types"].isArray())
      {
        for (const auto &type : json["types"])
        {
          result.types.push_back(type.asString());
        }
      }

      if (json.isMember("values") && json["values"].isArray())
      {
        for (const auto &row : json["values"])
        {
          std::vector<Json::Value> rowValues;
          for (const auto &val : row)
          {
            rowValues.push_back(val);
          }
          result.values.push_back(rowValues);
        }
      }

      if (json.isMember("time"))
        result.time = json["time"].asDouble();

      if (json.isMember("error"))
        result.error = json["error"].asString();

      return result;
    }
  };

  struct QueryResultAssoc
  {
    std::map<std::string, std::string> types;
    std::vector<std::map<std::string, Json::Value>> rows;
    double time = 0.0;
    std::string error;

    static QueryResultAssoc fromJson(const Json::Value &json)
    {
      QueryResultAssoc result;

      if (json.isMember("types") && json["types"].isObject())
      {
        for (const auto &typeName : json["types"].getMemberNames())
        {
          result.types[typeName] = json["types"][typeName].asString();
        }
      }

      if (json.isMember("rows") && json["rows"].isArray())
      {
        for (const auto &rowJson : json["rows"])
        {
          std::map<std::string, Json::Value> row;
          for (const auto &fieldName : rowJson.getMemberNames())
          {
            row[fieldName] = rowJson[fieldName];
          }
          result.rows.push_back(row);
        }
      }

      if (json.isMember("time"))
        result.time = json["time"].asDouble();

      if (json.isMember("error"))
        result.error = json["error"].asString();

      return result;
    }
  };

  struct QueryResponse
  {
    std::vector<QueryResult> results;
    std::vector<QueryResultAssoc> resultsAssoc;
    double time = 0.0;
    bool isAssociative = false;

    static QueryResponse fromJson(const Json::Value &json)
    {
      QueryResponse response;

      if (json.isMember("time"))
        response.time = json["time"].asDouble();

      if (json.isMember("results") && json["results"].isArray())
      {
        // Determine if we have associative results by checking the first element
        const Json::Value &firstResult = json["results"][0];
        if (firstResult.isMember("rows"))
        {
          response.isAssociative = true;
          for (const auto &resultJson : json["results"])
          {
            response.resultsAssoc.push_back(QueryResultAssoc::fromJson(resultJson));
          }
        }
        else
        {
          for (const auto &resultJson : json["results"])
          {
            response.results.push_back(QueryResult::fromJson(resultJson));
          }
        }
      }

      return response;
    }

    bool hasError() const
    {
      if (isAssociative)
      {
        for (const auto &result : resultsAssoc)
        {
          if (!result.error.empty())
          {
            return true;
          }
        }
      }
      else
      {
        for (const auto &result : results)
        {
          if (!result.error.empty())
          {
            return true;
          }
        }
      }
      return false;
    }
  };

  struct RequestResult
  {
    std::vector<std::string> columns;
    std::vector<std::string> types;
    std::vector<std::vector<Json::Value>> values;
    int64_t lastInsertID = 0;
    int64_t rowsAffected = 0;
    std::string error;
    double time = 0.0;

    static RequestResult fromJson(const Json::Value &json)
    {
      RequestResult result;

      if (json.isMember("columns") && json["columns"].isArray())
      {
        for (const auto &col : json["columns"])
        {
          result.columns.push_back(col.asString());
        }
      }

      if (json.isMember("types") && json["types"].isArray())
      {
        for (const auto &type : json["types"])
        {
          result.types.push_back(type.asString());
        }
      }

      if (json.isMember("values") && json["values"].isArray())
      {
        for (const auto &row : json["values"])
        {
          std::vector<Json::Value> rowValues;
          for (const auto &val : row)
          {
            rowValues.push_back(val);
          }
          result.values.push_back(rowValues);
        }
      }

      if (json.isMember("last_insert_id"))
        result.lastInsertID = json["last_insert_id"].asInt64();

      if (json.isMember("rows_affected"))
        result.rowsAffected = json["rows_affected"].asInt64();

      if (json.isMember("error"))
        result.error = json["error"].asString();

      if (json.isMember("time"))
        result.time = json["time"].asDouble();

      return result;
    }
  };

  struct RequestResultAssoc
  {
    std::map<std::string, std::string> types;
    std::vector<std::map<std::string, Json::Value>> rows;
    int64_t lastInsertID = 0;
    int64_t rowsAffected = 0;
    std::string error;
    double time = 0.0;

    static RequestResultAssoc fromJson(const Json::Value &json)
    {
      RequestResultAssoc result;

      if (json.isMember("types") && json["types"].isObject())
      {
        for (const auto &typeName : json["types"].getMemberNames())
        {
          result.types[typeName] = json["types"][typeName].asString();
        }
      }

      if (json.isMember("rows") && json["rows"].isArray())
      {
        for (const auto &rowJson : json["rows"])
        {
          std::map<std::string, Json::Value> row;
          for (const auto &fieldName : rowJson.getMemberNames())
          {
            row[fieldName] = rowJson[fieldName];
          }
          result.rows.push_back(row);
        }
      }

      if (json.isMember("last_insert_id"))
        result.lastInsertID = json["last_insert_id"].asInt64();

      if (json.isMember("rows_affected"))
        result.rowsAffected = json["rows_affected"].asInt64();

      if (json.isMember("error"))
        result.error = json["error"].asString();

      if (json.isMember("time"))
        result.time = json["time"].asDouble();

      return result;
    }
  };

  struct RequestResponse
  {
    std::vector<RequestResult> results;
    std::vector<RequestResultAssoc> resultsAssoc;
    double time = 0.0;
    bool isAssociative = false;

    static RequestResponse fromJson(const Json::Value &json)
    {
      RequestResponse response;

      if (json.isMember("time"))
        response.time = json["time"].asDouble();

      if (json.isMember("results") && json["results"].isArray())
      {
        // Determine if we have associative results by checking the first element
        const Json::Value &firstResult = json["results"][0];
        if (firstResult.isMember("rows"))
        {
          response.isAssociative = true;
          for (const auto &resultJson : json["results"])
          {
            response.resultsAssoc.push_back(RequestResultAssoc::fromJson(resultJson));
          }
        }
        else
        {
          for (const auto &resultJson : json["results"])
          {
            response.results.push_back(RequestResult::fromJson(resultJson));
          }
        }
      }

      return response;
    }

    bool hasError() const
    {
      if (isAssociative)
      {
        for (const auto &result : resultsAssoc)
        {
          if (!result.error.empty())
          {
            return true;
          }
        }
      }
      else
      {
        for (const auto &result : results)
        {
          if (!result.error.empty())
          {
            return true;
          }
        }
      }
      return false;
    }
  };

  // Main RqliteClient class
  class RqliteClient
  {
  public:
    RqliteClient(const std::string &baseURL, unsigned int timeout);
    ~RqliteClient();

    // Basic operations
    void setBasicAuth(const std::string &username, const std::string &password);
    void promoteErrors(bool value);

    // SQL operations
    ExecuteResponse executeSingle(const std::string &statement);

    template <typename... Args>
    ExecuteResponse executeSingle(const std::string &statement, Args... args)
    {
      SQLStatement stmt = SQLStatement::newSQLStatement(statement, args...);
      SQLStatements stmts;
      stmts.add(stmt);
      return execute(stmts, nullptr);
    }

    ExecuteResponse execute(const SQLStatements &statements, const ExecuteOptions *opts = nullptr);

    QueryResponse querySingle(const std::string &statement);

    template <typename... Args>
    QueryResponse querySingle(const std::string &statement, Args... args)
    {
      SQLStatement stmt = SQLStatement::newSQLStatement(statement, args...);
      SQLStatements stmts;
      stmts.add(stmt);
      return query(stmts, nullptr);
    }

    QueryResponse query(const SQLStatements &statements, const QueryOptions *opts = nullptr);

    RequestResponse requestSingle(const std::string &statement);

    template <typename... Args>
    RequestResponse requestSingle(const std::string &statement, Args... args)
    {
      SQLStatement stmt = SQLStatement::newSQLStatement(statement, args...);
      SQLStatements stmts;
      stmts.add(stmt);
      return request(stmts, nullptr);
    }

    RequestResponse request(const SQLStatements &statements, const RequestOptions *opts = nullptr);

    // Other database operations
    std::istream *backup(const BackupOptions *opts = nullptr);
    void load(std::istream &in, const LoadOptions *opts = nullptr);
    void boot(std::istream &in);

    // Cluster information
    Json::Value status();
    Json::Value expvar();
    Json::Value nodes();
    std::istream *ready();

  private:
    std::string executeURL;
    std::string queryURL;
    std::string requestURL;
    std::string backupURL;
    std::string loadURL;
    std::string bootURL;
    std::string statusURL;
    std::string expvarURL;
    std::string nodesURL;
    std::string readyURL;

    std::string basicAuthUser;
    std::string basicAuthPass;
    bool promoteErrorsFlag;
    unsigned int timeOut;

    // Inner class to hold HTTP response data
    struct HttpResponse
    {
      int statusCode;
      std::string body;
      std::unique_ptr<std::stringstream> stream;
    };

    // Helper methods
    HttpResponse doJSONRequest(OrthancPluginHttpMethod method, const std::string &url, const std::string &body);
    HttpResponse doGetRequest(const std::string &url);
    std::string buildAuthHeader() const;
    bool validSQLiteData(const std::vector<char> &data) const;
  };


} // namespace rqlite