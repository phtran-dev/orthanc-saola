#include "AppConfigRepository.h"

#include <EmbeddedResources.h>

namespace Saola
{
  AppConfigRepository& AppConfigRepository::Instance()
  {
    static AppConfigRepository instance;
    return instance;
  }

  AppConfigRepository::AppConfigRepository()
  {
    OrthancPlugins::OrthancConfiguration configuration;
    OrthancPlugins::OrthancConfiguration saola;
    configuration.GetSection(saola, "Saola");
    std::string appConfigDataSourceUrl = saola.GetStringValue("AppConfig.DataSource.Url", "");;
    if (!appConfigDataSourceUrl.empty())
    {
      LOG(ERROR) << "[AppConfigRepository] ERROR AppConfig.DataSource.Url is not configured";
      isMemoryMode_ = false;
    }

    if (isMemoryMode_)
    {
      OpenInMemory(saola);
    }
    else
    {
      OpenDB(saola);
    }
  }

  void AppConfigRepository::OpenInMemory(const OrthancPlugins::OrthancConfiguration& saola)
  {
    LOG(INFO) << "[AppConfigRepository] Opening in memory";
    if (saola.GetJson().isMember("Apps"))
    {
      const Json::Value& apps = saola.GetJson()["Apps"];
      for (const auto &appConfig : apps)
      {
        // Validate configurations
        if (!appConfig.isMember("Id") || !appConfig.isMember("Type") || !appConfig.isMember("Url") || !appConfig.isMember("Enable"))
        {
          LOG(ERROR) << "[MemAppConfigRepository] ERROR Missing mandatory configurations: Id, Type, Url, Enable: " << appConfig.toStyledString();
          continue;
        }

        if (!appConfig["Enable"].asBool())
        {
          continue;
        }

        std::string id = appConfig["Id"].asString();
        if (inMemoryAppConfigs_.find(id) != inMemoryAppConfigs_.end())
        {
          inMemoryAppConfigs_.erase(id);
        }

        inMemoryAppConfigs_.emplace(id, std::make_shared<AppConfiguration>(appConfig));
      }
    }
  }

  void AppConfigRepository::OpenDB(const OrthancPlugins::OrthancConfiguration& saola)
  {
    LOG(INFO) << "[AppConfigRepository] Opening in database";
    std::string url = saola.GetStringValue("AppConfig.DataSource.Url", "");
    int timeout = saola.GetIntegerValue("AppConfig.DataSource.Timeout", 5);
    rqliteClient_.reset(new rqlite::RqliteClient(url, timeout));
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
    }

    if (!existing)
    {
      std::string sql;
      Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::PREPARE_APPCONFIG_DATABASE);
      rqliteClient_->executeSingle(sql);
    }
  }

  std::shared_ptr<AppConfiguration> AppConfigRepository::Get(const std::string& id)
  {
    if (isMemoryMode_)
    {
      return inMemoryAppConfigs_.find(id) != inMemoryAppConfigs_.end() ? inMemoryAppConfigs_[id] : nullptr;
    }

    // Getting from Database
    try 
    {
      std::string sql = "SELECT Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback FROM AppConfiguration WHERE id=?";
      LOG(INFO) << "[AppConfigRepository] Getting from database: " << sql;
      rqlite::QueryResponse queryResp = rqliteClient_->querySingle(sql, id);

      Json::Value appConfigJson;
      for (const auto& result : queryResp.results)
      {
        for (const auto& row : result.values) 
        {
          appConfigJson["Id"] = row[0];
          if (!row[1].isNull())
          {
            appConfigJson["Enable"] = row[1].asString() == "true";
          }
          else
          {
            appConfigJson["Enable"] = false;
          }
          appConfigJson["Type"] = row[2];
          appConfigJson["Delay"] = row[3];
          appConfigJson["Url"] = row[4];
          appConfigJson["Authentication"] = row[5];
          appConfigJson["Method"] = row[6];
          appConfigJson["Timeout"] = row[7];
          if (!row[8].isNull())
          {
            appConfigJson["FieldMappingOverwrite"] = row[8].asString() == "true";
          }
          else
          {
            appConfigJson["FieldMappingOverwrite"] = false;
          }
          
          if (!row[9].isNull() && !row[9].empty() && !row[9].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(appConfigJson["FieldMapping"], row[9].asString());
          }

          if (!row[10].isNull() && !row[10].empty() && !row[10].asString().empty())
          {
            Orthanc::Toolbox::ReadJson(appConfigJson["FieldValues"], row[10].asString());
          }

          appConfigJson["LuaCallback"] = row[11];
        }
      }

      if (!appConfigJson.empty())
      {
        return std::make_shared<AppConfiguration>(appConfigJson);
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[DBAppConfigRepository::Get] ERROR Cannot get id="<< id << " from AppConfiguration err=" << e.What(); 
    }
    
    return nullptr;
  }

  void AppConfigRepository::Get(const std::string& id, Json::Value& value)
  {
    auto config = Get(id);
    if (config)
    {
      config->ToJson(value);
    }
  }

  std::map<std::string, std::shared_ptr<AppConfiguration>> AppConfigRepository::GetAll()
  {
    if (isMemoryMode_)
    {
      return inMemoryAppConfigs_;
    }
    
    std::map<std::string, std::shared_ptr<AppConfiguration>> appConfigs;
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
          auto appConfig = std::make_shared<AppConfiguration>(config);
          appConfigs.emplace(appConfig->id_, appConfig);
        }
      }
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[AppConfigDatabase::GetAppConfigs] ERROR Cannot get AppConfiguration err=" << e.What(); 
    }
    
    return appConfigs;
  }

  void AppConfigRepository::GetAll(Json::Value& value)
  {
    const auto& appConfigs = GetAll();
    for (const auto& appConfig : appConfigs)
    {
      Json::Value config;
      appConfig.second->ToJson(config);
      value.append(config);
    }
  }

  void AppConfigRepository::Save(const AppConfiguration& appConfig)
  {
    if (isMemoryMode_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      inMemoryAppConfigs_[appConfig.id_] = std::make_shared<AppConfiguration>(appConfig);
      return;
    }
    
    Json::Value appConfigJson;
    appConfig.ToJson(appConfigJson);

    std::string id = appConfigJson["Id"].asString();
    bool enable = appConfigJson.isMember("Enable") ? appConfigJson["Enable"].asBool() : false;
    std::string type = appConfigJson.isMember("Type") ? appConfigJson["Type"].asString() : "";
    int delay = appConfigJson.isMember("Delay") ? appConfigJson["Delay"].asInt() : 0;
    std::string url = appConfigJson.isMember("Url") ? appConfigJson["Url"].asString() : "";
    std::string authentication = appConfigJson.isMember("Authentication") ? appConfigJson["Authentication"].asString() : "";
    std::string method = appConfigJson.isMember("Method") ? appConfigJson["Method"].asString() : "POST";
    int timeout = appConfigJson.isMember("Timeout") ? appConfigJson["Timeout"].asInt() : 60;
    bool fieldMappingOverwrite = appConfigJson.isMember("FieldMappingOverwrite") ? appConfigJson["FieldMappingOverwrite"].asBool() : false;

    std::string fieldMapping = "";
    if (appConfigJson.isMember("FieldMapping"))
    {
      Orthanc::Toolbox::WriteFastJson(fieldMapping, appConfigJson["FieldMapping"]);
    }

    std::string fieldValues = "";
    if (appConfigJson.isMember("FieldValues"))
    {
      Orthanc::Toolbox::WriteFastJson(fieldValues, appConfigJson["FieldValues"]);
    }

    std::string luaCallback = appConfigJson.isMember("LuaCallback") ? appConfigJson["LuaCallback"].asString() : "";

    try 
    {
      // Use INSERT OR REPLACE as requested in previous steps, matching current AppConfigDatabase evolution
      std::string sql = "INSERT OR REPLACE INTO AppConfiguration (Id, Enable, Type, Delay, Url, Authentication, Method, Timeout, FieldMappingOverwrite, FieldMapping, FieldValues, LuaCallback) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"; 
      rqlite::ExecuteResponse insertResp = rqliteClient_->executeSingle(sql, id, enable, type, delay, url, authentication, method, timeout, fieldMappingOverwrite, fieldMapping, fieldValues, luaCallback);
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[DBAppConfigRepository::Save] ERROR Cannot save AppConfiguration err=" << e.What(); 
    }
  }

  void AppConfigRepository::Delete(const std::string& id)
  {
    if (isMemoryMode_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      inMemoryAppConfigs_.erase(id);
      return;
    }
    
    try 
    {
      std::string sql = "DELETE FROM AppConfiguration WHERE id=?";
      rqlite::ExecuteResponse deleteResp = rqliteClient_->executeSingle(sql, id);
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[DBAppConfigRepository::Delete] ERROR Cannot delete id="<< id << " from AppConfiguration err=" << e.What(); 
    }
  }

  void AppConfigRepository::DeleteAll()
  {
    if (isMemoryMode_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      inMemoryAppConfigs_.clear();
      return;
    }
    
    try 
    {
      std::string sql = "DELETE FROM AppConfiguration";
      rqlite::ExecuteResponse deleteResp = rqliteClient_->executeSingle(sql);
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << "[DBAppConfigRepository::DeleteAll] ERROR Cannot delete all from AppConfiguration err=" << e.What(); 
    }
  }
}
