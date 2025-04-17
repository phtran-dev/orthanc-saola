#include "AppConfigDatabase.h"
#include "RQLite.h"


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
      std::string sql = "SELECT name FROM sqlite_master WHERE name=?";
      rqlite::QueryResponse queryResp = rqliteClient_->querySingle(sql, "AppConfiguration");

      for (const auto& result : queryResp.results)
      {
        for (const auto& row : result.values) 
        {
            for (const auto& value : row) 
            {
              if (value.asString() == "AppConfiguration")
              {
                existing = true;
              }
            }
        }
      }



      // std::string encoded_sql;
      // Orthanc::Toolbox::UriEncode(encoded_sql, "SELECT name FROM sqlite_master WHERE name=\"AppConfiguration\"");
      // client_.SetUrl(this->url_ + "/db/query?q=" + encoded_sql);
      // client_.SetMethod(OrthancPluginHttpMethod_Get);
      // Json::Value answerBody;
      // OrthancPlugins::HttpClient::HttpHeaders answerHeaders;
      // client_.Execute(answerHeaders, answerBody);      

      // if (answerBody.isMember("results"))
      // {
      //   auto& results = answerBody["results"];
      //   if (results.isArray() && results.size() > 0)
      //   {
      //     if (!results[0].isMember("error") && results[0].isMember("values"))
      //     {
      //       for (const auto& values : results[0]["values"])
      //       {
      //         for (const auto& value : values)
      //         {
      //           if (value.asString() == "AppConfiguration")
      //           {
      //             existing = true;
      //           }
      //         }
      //       }
      //     }
      //   }
      // }
    }

    if (!existing)
    {
      std::string sql;
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_APPCONFIG_DATABASE);
      rqliteClient_->executeSingle(sql);
    }
  }

  void AppConfigDatabase::Open(const std::string& url, int timeout)
  {
    boost::mutex::scoped_lock lock(mutex_);
    this->enabled_ = true;
    this->url_ = url;
    this->timeout_ = timeout;
    rqliteClient_.reset(new rqlite::RqliteClient(url, timeout));
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
      rqlite::QueryResponse queryResp = rqliteClient_->querySingle(sql);
      for (const auto& result : queryResp.results)
      {
        for (const auto& row : result.values) 
        {
          Json::Value config;
          config["Id"] = row[0];
          if (!row[1].isNull())
          {
            config["Enable"] = row[1].asString() == "true";
          }
          else
          {
            config["Enable"] = false;
          }
          config["Type"] = row[2];
          config["Delay"] = row[3];
          config["Url"] = row[4];
          config["Authentication"] = row[5];
          config["Method"] = row[6];
          config["Timeout"] = row[7];
          if (!row[8].isNull())
          {
            config["FieldMappingOverwrite"] = row[8].asString() == "true";
          }
          else
          {
            config["FieldMappingOverwrite"] = false;
          }
          
          if (!row[9].isNull() && !row[9].empty() && !row[9].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(config["FieldMapping"], row[9].asString());
          }

          if (!row[10].isNull() && !row[10].empty() && !row[10].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(config["FieldValues"], row[10].asString());
          }

          config["LuaCallback"] = row[11];
          appConfigs.append(config);
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
      std::string sql = "SELECT Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback FROM AppConfiguration WHERE id=?";
      rqlite::QueryResponse queryResp = rqliteClient_->querySingle(sql, id);

      for (const auto& result : queryResp.results)
      {
        for (const auto& row : result.values) 
        {
          appConfig["Id"] = row[0];
          if (!row[1].isNull())
          {
            appConfig["Enable"] = row[1].asString() == "true";
          }
          else
          {
            appConfig["Enable"] = false;
          }
          appConfig["Type"] = row[2];
          appConfig["Delay"] = row[3];
          appConfig["Url"] = row[4];
          appConfig["Authentication"] = row[5];
          appConfig["Method"] = row[6];
          appConfig["Timeout"] = row[7];
          if (!row[8].isNull())
          {
            appConfig["FieldMappingOverwrite"] = row[8].asString() == "true";
          }
          else
          {
            appConfig["FieldMappingOverwrite"] = false;
          }
          
          if (!row[9].isNull() && !row[9].empty() && !row[9].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(appConfig["FieldMapping"], row[9].asString());
          }

          if (!row[10].isNull() && !row[10].empty() && !row[10].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(appConfig["FieldValues"], row[10].asString());
          }

          appConfig["LuaCallback"] = row[11];
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
      std::string sql = "INSERT INTO AppConfiguration (Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"; 
      rqlite::ExecuteResponse insertResp = rqliteClient_->executeSingle(sql, id, enable, type, delay, url, authentication, method, timeout, fieldMappingOverwrite, fieldMapping, fieldValues, luaCallback);
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
      std::string sql = "DELETE FROM AppConfiguration WHERE id=?";
      rqlite::ExecuteResponse deleteResp = rqliteClient_->executeSingle(sql, id);
      return !deleteResp.hasError();
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[AppConfigDatabase::DeleteAppConfigById] ERROR Cannot get id="<< id << " from AppConfiguration err=" << e.What(); 
    }
    
    return false;
  }

}