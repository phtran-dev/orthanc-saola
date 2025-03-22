  #include "AppConfigDatabase.h"

  #include <EmbeddedResources.h>

  #include <Toolbox.h>
  #include <Logging.h>

  #include <boost/algorithm/string.hpp>

namespace Saola
{
  AppConfigDatabase& AppConfigDatabase::Instance()
  {
    static AppConfigDatabase db;
    return db;
  }

  void AppConfigDatabase::Initialize()
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
                if (value.asString() == "AppConfiguration")
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
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_APPCONFIG_DATABASE);
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


  void AppConfigDatabase::Open(const std::string& url)
  {
    boost::mutex::scoped_lock lock(mutex_);
    this->url_ = url;
    Initialize();
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

  void AppConfigDatabase::GetAppConfigs(Json::Value& appConfigs)
  {
    try 
    {
      std::string sql = "SELECT Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback FROM AppConfiguration";

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
              if (!value[1].isNull())
              {
                config["Enable"] = value[1].asString() == "true";
              }
              else
              {
                config["Enable"] = false;
              }
              config["Type"] = value[2];
              config["Delay"] = value[3];
              config["Url"] = value[4];
              config["Authentication"] = value[5];
              config["Method"] = value[6];
              config["Timeout"] = value[7];
              if (!value[8].isNull())
              {
                config["FieldMappingOverwrite"] = value[8].asString() == "true";
              }
              else
              {
                config["FieldMappingOverwrite"] = false;
              }

              Orthanc::Toolbox::ReadJson(config["FieldMapping"], value[9].asString());
              Orthanc::Toolbox::ReadJson(config["FieldValues"], value[10].asString());
              
              config["LuaCallback"] = value[11];
              appConfigs.append(config);
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

  void AppConfigDatabase::GetAppConfigById(Json::Value& serverConfig, const std::string& id)
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

  
  void AppConfigDatabase::SaveAppConfig(Json::Value& serverConfig)
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

  bool AppConfigDatabase::DeleteAppConfigById(const std::string& id)
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
      LOG(ERROR) << "[AppConfigDatabase::DeleteAppConfigById] Cannot get id="<< id << " from DicomServer err=" << e.What(); 
    }
    
    return false;
  }

}