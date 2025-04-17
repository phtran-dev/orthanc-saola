// rqlite.cpp
#include "RQLite.h"

#include <Toolbox.h>

namespace rqlite
{

  // Convert ReadConsistencyLevel to string
  std::string toString(ReadConsistencyLevel level)
  {
    switch (level)
    {
    case ReadConsistencyLevel::NONE:
      return "none";
    case ReadConsistencyLevel::WEAK:
      return "weak";
    case ReadConsistencyLevel::STRONG:
      return "strong";
    case ReadConsistencyLevel::LINEARIZABLE:
      return "linearizable";
    case ReadConsistencyLevel::AUTO:
      return "auto";
    default:
      return "unknown";
    }
  }

  RqliteClient::RqliteClient(const std::string &baseURL, unsigned int timeOut) : promoteErrorsFlag(false)
  {
    executeURL = baseURL + "/db/execute";
    queryURL = baseURL + "/db/query";
    requestURL = baseURL + "/db/request";
    backupURL = baseURL + "/db/backup";
    loadURL = baseURL + "/db/load";
    bootURL = baseURL + "/boot";
    statusURL = baseURL + "/status";
    expvarURL = baseURL + "/debug/vars";
    nodesURL = baseURL + "/nodes";
    readyURL = baseURL + "/readyz";
    this->timeOut = timeOut;

    // Initialize Orthanc HttpClient
  }

  RqliteClient::~RqliteClient()
  {
    // httpClient will be automatically deleted by unique_ptr
  }

  void RqliteClient::setBasicAuth(const std::string &username, const std::string &password)
  {
    basicAuthUser = username;
    basicAuthPass = password;
  }

  void RqliteClient::promoteErrors(bool value)
  {
    promoteErrorsFlag = value;
  }

  std::string RqliteClient::buildAuthHeader() const
  {
    if (basicAuthUser.empty() && basicAuthPass.empty())
    {
      return "";
    }

    std::string auth = basicAuthUser + ":" + basicAuthPass;
    std::string encoded;
    Orthanc::Toolbox::EncodeBase64(encoded, auth);
    return "Basic " + encoded;
  }

  ExecuteResponse RqliteClient::executeSingle(const std::string &statement)
  {
    SQLStatement stmt(statement);
    SQLStatements stmts;
    stmts.add(stmt);
    return execute(stmts, nullptr);
  }

  ExecuteResponse RqliteClient::execute(const SQLStatements &statements, const ExecuteOptions *opts)
  {
    std::string url = executeURL;
    if (opts)
    {
      url += opts->toQueryString();
    }

    HttpResponse resp = doJSONRequest(OrthancPluginHttpMethod_Post, url, statements.toJson().toStyledString());
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode) +
                               ", body: " + resp.body);
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    ExecuteResponse response = ExecuteResponse::fromJson(root);
    if (promoteErrorsFlag && response.hasError())
    {
      throw std::runtime_error("Statement error encountered");
    }

    return response;
  }

  QueryResponse RqliteClient::querySingle(const std::string &statement)
  {
    SQLStatement stmt(statement);
    SQLStatements stmts;
    stmts.add(stmt);
    return query(stmts, nullptr);
  }

  QueryResponse RqliteClient::query(const SQLStatements &statements, const QueryOptions *opts)
  {
    std::string url = queryURL;
    if (opts)
    {
      url += opts->toQueryString();
    }

    HttpResponse resp = doJSONRequest(OrthancPluginHttpMethod_Post, url, statements.toJson().toStyledString());
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode) +
                               ", body: " + resp.body);
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    QueryResponse response = QueryResponse::fromJson(root);
    if (promoteErrorsFlag && response.hasError())
    {
      throw std::runtime_error("Query error encountered");
    }

    return response;
  }

  RequestResponse RqliteClient::requestSingle(const std::string &statement)
  {
    SQLStatement stmt(statement);
    SQLStatements stmts;
    stmts.add(stmt);
    return request(stmts, nullptr);
  }

  RequestResponse RqliteClient::request(const SQLStatements &statements, const RequestOptions *opts)
  {
    std::string url = requestURL;
    if (opts)
    {
      url += opts->toQueryString();
    }

    HttpResponse resp = doJSONRequest(OrthancPluginHttpMethod_Post, url, statements.toJson().toStyledString());
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode) +
                               ", body: " + resp.body);
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    RequestResponse response = RequestResponse::fromJson(root);
    if (promoteErrorsFlag && response.hasError())
    {
      throw std::runtime_error("Request error encountered");
    }

    return response;
  }

  std::istream *RqliteClient::backup(const BackupOptions *opts)
  {
    std::string url = backupURL;
    if (opts)
    {
      url += opts->toQueryString();
    }

    HttpResponse resp = doGetRequest(url);
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode));
    }

    return resp.stream.release(); // Transfer ownership to caller
  }

  void RqliteClient::load(std::istream &in, const LoadOptions *opts)
  {
    std::string url = loadURL;
    if (opts)
    {
      url += opts->toQueryString();
    }

    // Read first 13 bytes to check if it's SQLite format
    std::vector<char> first13(13);
    in.read(first13.data(), 13);

    // Create a content string from the input stream
    std::stringstream contentStream;
    contentStream.write(first13.data(), in.gcount()); // Write what we actually read
    contentStream << in.rdbuf();                      // Append the rest of the stream
    std::string content = contentStream.str();

    OrthancPlugins::HttpClient httpClient;
    httpClient.SetMethod(OrthancPluginHttpMethod_Post);
    httpClient.SetUrl(url);
    httpClient.SetBody(content);
    httpClient.SetTimeout(this->timeOut);

    if (validSQLiteData(first13))
    {
      // Set content type to application/octet-stream for SQLite data
      httpClient.AddHeader("Content-Type", "application/octet-stream");

      // std::string authHeader = buildAuthHeader();
      // if (!authHeader.empty()) {
      //     httpClient->AddHeader("Authorization", authHeader);
      // }
    }
    else
    {
      // Set content type to text/plain for SQL statements
      httpClient.AddHeader("Content-Type", "text/plain");

      // std::string authHeader = buildAuthHeader();
      // if (!authHeader.empty()) {
      //     httpClient->AddHeader("Authorization", authHeader);
      // }
    }

    HttpResponse resp;
    OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
    httpClient.Execute(answerHeaders, resp.body);
    resp.statusCode = httpClient.GetHttpStatus();

    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode) +
                               ", body: " + resp.body);
    }
  }

  void RqliteClient::boot(std::istream &in)
  {
    // Read all content from the input stream
    std::stringstream contentStream;
    contentStream << in.rdbuf();
    std::string content = contentStream.str();

    OrthancPlugins::HttpClient httpClient;
    httpClient.SetMethod(OrthancPluginHttpMethod_Post);
    httpClient.SetUrl(bootURL);
    httpClient.SetBody(content);
    httpClient.AddHeader("Content-Type", "application/octet-stream");
    httpClient.SetTimeout(this->timeOut);

    // std::string authHeader = buildAuthHeader();
    // if (!authHeader.empty()) {
    //     httpClient->AddHeader("Authorization", authHeader);
    // }

    httpClient.Execute();

    int statusCode = httpClient.GetHttpStatus();
    if (statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(statusCode));
    }
  }

  Json::Value RqliteClient::status()
  {
    HttpResponse resp = doGetRequest(statusURL);
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode));
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    return root;
  }

  Json::Value RqliteClient::expvar()
  {
    HttpResponse resp = doGetRequest(expvarURL);
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode));
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    return root;
  }

  Json::Value RqliteClient::nodes()
  {
    HttpResponse resp = doGetRequest(nodesURL);
    if (resp.statusCode != 200)
    {
      throw std::runtime_error("Unexpected status code: " + std::to_string(resp.statusCode));
    }

    Json::Value root;
    if (!OrthancPlugins::ReadJson(root, resp.body))
    {
      throw std::runtime_error("Failed to parse JSON response");
    }

    return root;
  }

  std::istream *RqliteClient::ready()
  {
    HttpResponse resp = doGetRequest(readyURL);
    return resp.stream.release(); // Transfer ownership to caller
  }

  RqliteClient::HttpResponse RqliteClient::doJSONRequest(OrthancPluginHttpMethod method, const std::string &url, const std::string &body)
  {
    OrthancPlugins::HttpClient httpClient;
    httpClient.SetUrl(url);
    httpClient.SetMethod(method);
    httpClient.SetBody(body);
    httpClient.AddHeader("Content-Type", "application/json");
    httpClient.SetTimeout(this->timeOut);

    // std::string authHeader = buildAuthHeader();
    // if (!authHeader.empty()) {
    //     httpClient->AddHeader("Authorization", authHeader);
    // }

    HttpResponse response;
    OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
    httpClient.Execute(answerHeaders, response.body);

    response.statusCode = httpClient.GetHttpStatus();
    return response;
  }

  RqliteClient::HttpResponse RqliteClient::doGetRequest(const std::string &url)
  {
    OrthancPlugins::HttpClient httpClient;
    httpClient.SetMethod(OrthancPluginHttpMethod_Get);
    httpClient.SetUrl(url);
    httpClient.SetTimeout(this->timeOut);

    // std::string authHeader = buildAuthHeader();
    // if (!authHeader.empty()) {
    //     httpClient->AddHeader("Authorization", authHeader);
    // }

    HttpResponse response;
    OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
    httpClient.Execute(answerHeaders, response.body);

    // For methods that might want a stream
    response.stream.reset(new std::stringstream(response.body));

    return response;
  }

  bool RqliteClient::validSQLiteData(const std::vector<char> &data) const
  {
    if (data.size() < 13)
    {
      return false;
    }

    std::string header(data.begin(), data.begin() + 13);
    return header == "SQLite format";
  }

} // namespace rqlite