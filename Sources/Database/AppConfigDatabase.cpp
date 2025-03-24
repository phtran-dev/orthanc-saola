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

  bool AppConfigDatabase::IsEnabled() const
  {
    return this->enabled_;
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
    this->enabled_ = true;
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
              
              if (!value[9].isNull() && !value[9].empty() && !value[9].asString().empty())
              {
                Orthanc::Toolbox::ReadJson(config["FieldMapping"], value[9].asString());
              }

              if (!value[10].isNull() && !value[10].empty() && !value[10].asString().empty())
              {
                Orthanc::Toolbox::ReadJson(config["FieldValues"], value[10].asString());
              }

              config["LuaCallback"] = value[11];
              appConfigs.append(config);
            }
          }
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[AppConfigDatabase::GetAppConfigs] ERROR Cannot get AppConfiguration err=" << e.What(); 
    }
  }

  void AppConfigDatabase::GetAppConfigById(Json::Value& appConfig, const std::string& id)
  {
    try 
    {
      Json::Value sql = Json::arrayValue;
      sql.append("SELECT Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback FROM AppConfiguration WHERE id=?");
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
              appConfig["Id"] = value[0];
              if (!value[1].isNull())
              {
                appConfig["Enable"] = value[1].asString() == "true";
              }
              else
              {
                appConfig["Enable"] = false;
              }
              appConfig["Type"] = value[2];
              appConfig["Delay"] = value[3];
              appConfig["Url"] = value[4];
              appConfig["Authentication"] = value[5];
              appConfig["Method"] = value[6];
              appConfig["Timeout"] = value[7];
              if (!value[8].isNull())
              {
                appConfig["FieldMappingOverwrite"] = value[8].asString() == "true";
              }
              else
              {
                appConfig["FieldMappingOverwrite"] = false;
              }

              Orthanc::Toolbox::ReadJson(appConfig["FieldMapping"], value[9].asString());
              Orthanc::Toolbox::ReadJson(appConfig["FieldValues"], value[10].asString());
              
              appConfig["LuaCallback"] = value[11];
            }
          }
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[AppConfigDatabase::GetAppConfigById] ERROR Cannot get id="<< id << " from AppConfiguration err=" << e.What(); 
    }
  }

  
  void AppConfigDatabase::SaveAppConfig(const Json::Value& appConfig)
  {
    boost::mutex::scoped_lock lock(mutex_);
    std::string id = appConfig["Id"].asString();
    bool enable = appConfig.isMember("Enable") ? appConfig["Enable"].asBool() : false;
    std::string type = appConfig.isMember("Type") ? appConfig["Type"].asString() : "";
    int delay = appConfig.isMember("Delay") ? appConfig["Delay"].asInt() : 0;
    std::string url = appConfig.isMember("Url") ? appConfig["Url"].asString() : "";
    std::string authentication = appConfig.isMember("Authentication") ? appConfig["Authentication"].asString() : "";
    std::string method = appConfig.isMember("Method") ? appConfig["Method"].asString() : "POST";
    int timeout = appConfig.isMember("Timeout") ? appConfig["Timeout"].asInt() : 60;
    bool fieldMappingOverwrite = appConfig.isMember("FieldMappingOverwrite") ? appConfig["FieldMappingOverwrite"].asBool() : false;

    std::string fieldMapping = "";
    if (appConfig.isMember("FieldMapping"))
    {
      Orthanc::Toolbox::WriteFastJson(fieldMapping, appConfig["FieldMapping"]);
    }

    std::string fieldValues = "";
    if (appConfig.isMember("FieldValues"))
    {
      Orthanc::Toolbox::WriteFastJson(fieldValues, appConfig["FieldValues"]);
    }

    std::string luaCallback = appConfig.isMember("LuaCallback") ? appConfig["LuaCallback"].asString() : "";

    try 
    {
      Json::Value innerSQL = Json::arrayValue;
      innerSQL.append("INSERT INTO AppConfiguration (Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
      innerSQL.append(id);
      innerSQL.append(enable);
      innerSQL.append(type);
      innerSQL.append(delay);
      innerSQL.append(url);
      innerSQL.append(authentication);
      innerSQL.append(method);
      innerSQL.append(timeout);
      innerSQL.append(fieldMappingOverwrite);
      innerSQL.append(fieldMapping);
      innerSQL.append(fieldValues);
      innerSQL.append(luaCallback);

      Json::Value requestBody = Json::arrayValue;
      requestBody.append(innerSQL);

      client_.SetUrl(this->url_ + "/db/execute");
      client_.SetMethod(OrthancPluginHttpMethod_Post);

      client_.AddHeader("Content-Type", "application/json");
      client_.SetBody(requestBody.toStyledString());


      Json::Value answerBody;
      OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      client_.Execute(answerHeaders, answerBody);
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[ConfigDatabase::SaveDicomServer] ERROR Cannot get DicomServer err=" << e.What(); 
    }
  }

  bool AppConfigDatabase::DeleteAppConfigById(const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);

    try 
    {
      Json::Value sql = Json::arrayValue;
      sql.append("DELETE FROM AppConfiguration WHERE id=?");
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
      LOG(ERROR) << "[AppConfigDatabase::DeleteAppConfigById] ERROR Cannot get id="<< id << " from AppConfiguration err=" << e.What(); 
    }
    
    return false;
  }

}