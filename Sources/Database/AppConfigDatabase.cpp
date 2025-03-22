  #include "AppConfigDatabase.h"

  #include <EmbeddedResources.h>

  #include <Toolbox.h>
  #include <Logging.h>

  #include <boost/algorithm/string.hpp>

namespace Itech
{
  AppConfigDatabase& AppConfigDatabase::Instance()
  {
    static AppConfigDatabase db;
    return db;
  }

  void ConfigDatabase::Initialize()
  {
    bool existing = false;

    {
      std::string encoded_sql;
      Orthanc::Toolbox::UriEncode(encoded_sql, "SELECT name FROM sqlite_master WHERE name=\"AppConfiguration\"");
      client_.SetUrl(this->url_ + "/db/query?q=" + encoded_sql);
      client_.SetMethod(OrthancPluginHttpMethod_Get);
      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);      

      if (answerBody.isMember("results"))
      {
        auto& results = answerBody["results"];
        if (results.isArray() && results.size() > 0)
        {
          if (!results[0].isMember("error") && results[0].isMember("values"))
          {
            for (const auto& values : results[0]["values"])
            {
              for (const auto& value : values)
              {
                if (value.asString() == "DicomServer")
                {
                  existing = true;
                }
              }
            }
          }
        }
      }
    }

    if (!existing)
    {
      std::string sql;
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_CONFIG_DATABASE);
      Json::Value innerSQL = Json::arrayValue;
      innerSQL.append(sql);
      client_.SetUrl(this->url_ + "/db/execute");
      client_.SetMethod(OrthancPluginHttpMethod_Post);

      Json::Value requestBody = Json::arrayValue;
      requestBody.append(innerSQL);
      client_.SetBody(requestBody.toStyledString());
      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
    }
  }


  void ConfigDatabase::Open(const std::string& url)
  {
    boost::mutex::scoped_lock lock(mutex_);
    this->url_ = url;
    // // db_.Open(path);
    // db_.OpenInMemory();


    Initialize();
  }
    

  void ConfigDatabase::OpenInMemory()
  {
    // boost::mutex::scoped_lock lock(mutex_);
    // db_.OpenInMemory();
    // Initialize();
  }
    
  void toJsonArray(Json::Value& json, const std::string& value, const char* delim)
  {
    if (value.empty())
      return;

    std::list<std::string> vals;
    boost::split(vals, value, boost::is_any_of(delim));

    for (const auto& val : vals)
    {
      json.append(val);
    }
  }

  std::string joinJsonArray(const Json::Value& json, const std::string& key, const char* delim)
  {
    if (!json.isMember(key) || !json[key].isArray())
      return "";

    std::list<std::string> res;
    for (const auto& val : json[key])
    {
      res.push_back(val.asCString());
    }
    return boost::join(res, ",");
  }

  void ConfigDatabase::GetDicomServerConfigs(Json::Value& appConfigs)
  {
    try 
    {
      std::string sql = "SELECT Id, Enable, Type, Delay, Url, Authentication, Method, FieldMappingOverwrite, FieldMapping, FieldValues FROM AppConfiguration";

      std::string encoded_sql;
      Orthanc::Toolbox::UriEncode(encoded_sql, sql);
      client_.SetUrl(this->url_ + "/db/query?q=" + encoded_sql);
      client_.SetMethod(OrthancPluginHttpMethod_Get);
      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
      if (answerBody.isMember("results"))
      {
        auto& results = answerBody["results"];
        if (results.isArray() && results.size() > 0)
        {
          if (!results[0].isMember("error") && results[0].isMember("values"))
          {
            for (const auto& value : results[0]["values"])
            {
              Json::Value config;
              config["Id"] = value[0];
              config["Enable"] = value[1];
              config["Type"] = value[2];
              config["Delay"] = value[3];
              config["Url"] = value[4];
              config["Authentication"] = value[5];
              config["Method"] = value[6];
              config["Timeout"] = value[7];
              config["FieldMappingOverwrite"] = value[8];
              toJsonArray(config["FieldMapping"], value[9].asString(), ",");
              config["FieldValues"] = value[10];
              config["LuaCallback"] = value[11];


              toJsonArray(config["Labels"], value[3].asString(), ",");
              config["LabelsConstraint"] = value[4];
              config["LabelsStoreLevels"] = Json::arrayValue;
              toJsonArray(config["LabelsStoreLevels"], value[5].asString(), ",");
              config["HospitalId"] = value[6];
              config["DepartmentId"] = value[7];
              serverConfigs.append(config);
            }
          }
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "Cannot get DicomServer " << e.What(); 
    }
    
  }

  void ConfigDatabase::GetDicomServerConfigById(Json::Value& serverConfig, const std::string& id)
  {
    try 
    {
      Json::Value sql = Json::arrayValue;
      sql.append("SELECT id, aet, port, labels, labelsConstraint, labelsStoreLevels, hospitalId, departmentId FROM DicomServer WHERE id=?");
      sql.append(id);

      Json::Value bodyRequest = Json::arrayValue;
      bodyRequest.append(sql);

      client_.SetUrl(this->url_ + "/db/query");
      client_.SetMethod(OrthancPluginHttpMethod_Post);
      client_.SetBody(bodyRequest.toStyledString());
      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
      if (answerBody.isMember("results"))
      {
        auto& results = answerBody["results"];
        if (results.isArray() && results.size() > 0)
        {
          if (!results[0].isMember("error"))
          {
            for (const auto& value : answerBody["results"][0]["values"])
            {
              Json::Value config;
              serverConfig["id"] = value[0];
              serverConfig["AET"] = value[1];
              serverConfig["Port"] = value[2];
              serverConfig["Labels"] = Json::arrayValue;
              toJsonArray(serverConfig["Labels"], value[3].asString(), ",");
              serverConfig["LabelsConstraint"] = value[4];
              serverConfig["LabelsStoreLevels"] = Json::arrayValue;
              toJsonArray(serverConfig["LabelsStoreLevels"], value[5].asString(), ",");
              serverConfig["HospitalId"] = value[6];
              serverConfig["DepartmentId"] = value[7];
            }
          }
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[ConfigDatabase::GetDicomServerConfigById] Cannot get id="<< id << " from DicomServer err=" << e.What(); 
    }
  }

  
  void ConfigDatabase::SaveDicomServer(Json::Value& serverConfig)
  {
    boost::mutex::scoped_lock lock(mutex_);
    std::string id = Orthanc::Toolbox::GenerateUuid();
    std::string aet = serverConfig["AET"].asString();
    int         port = serverConfig["Port"].asInt();
    std::string labels = joinJsonArray(serverConfig, "Labels", ",");
    std::string labelsConstraint = serverConfig["LabelsConstraint"].asString();
    std::string labelsStoreLevels = joinJsonArray(serverConfig, "LabelsStoreLevels", ",");
    std::string hospitalId = serverConfig["HospitalId"].asString();
    std::string departmentId = serverConfig["DepartmentId"].asString();

    try 
    {
      Json::Value innerSQL = Json::arrayValue;
      innerSQL.append("INSERT INTO DicomServer (id, aet, port, labels, labelsConstraint, LabelsStoreLevels, hospitalId, departmentId) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
      innerSQL.append(id);
      innerSQL.append(aet);
      innerSQL.append(port);
      innerSQL.append(labels);
      innerSQL.append(labelsConstraint);
      innerSQL.append(labelsStoreLevels);
      innerSQL.append(hospitalId);
      innerSQL.append(departmentId);

      Json::Value requestBody = Json::arrayValue;
      requestBody.append(innerSQL);

      client_.SetUrl(this->url_ + "/db/execute");
      client_.SetMethod(OrthancPluginHttpMethod_Post);

      client_.AddHeader("Content-Type", "application/json");
      client_.SetBody(requestBody.toStyledString());


      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
      serverConfig["id"] = id;    
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[ConfigDatabase::SaveDicomServer] Cannot get DicomServer err=" << e.What(); 
    }
  }

  bool ConfigDatabase::DeleteDicomServerConfigById(const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);

    try 
    {
      Json::Value sql = Json::arrayValue;
      sql.append("DELETE FROM DicomServer WHERE id=?");
      sql.append(id);

      Json::Value bodyRequest = Json::arrayValue;
      bodyRequest.append(sql);

      client_.SetUrl(this->url_ + "/db/execute");
      client_.SetMethod(OrthancPluginHttpMethod_Post);
      client_.SetBody(bodyRequest.toStyledString());
      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
      if (answerBody.isMember("results"))
      {
        auto& results = answerBody["results"];
        if (results.isArray() && results.size() > 0)
        {
          if (!results[0].isMember("error"))
          {
            return true;
          }
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[ConfigDatabase::GetDicomServerConfigById] Cannot get id="<< id << " from DicomServer err=" << e.What(); 
    }
    
    return false;
  }

}