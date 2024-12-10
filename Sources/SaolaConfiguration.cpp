#include "SaolaConfiguration.h"
#include "Constants.h"
#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>

#include <boost/algorithm/string.hpp>


SaolaConfiguration::SaolaConfiguration(/* args */)
{
  OrthancPlugins::OrthancConfiguration configuration;
  OrthancPlugins::OrthancConfiguration saola, ris;
  configuration.GetSection(saola, "Saola");

  this->enable_ = saola.GetBooleanValue("Enable", false);
  this->root_ = saola.GetStringValue("Root", "/saola/");
  this->maxRetry_ = saola.GetIntegerValue("MaxRetry", 5);
  
  for (const auto& appConfig : saola.GetJson()["Apps"])
  {
    if (!appConfig["Enable"].asBool())
    {
      continue;
    }

    std::shared_ptr<AppConfiguration> app = std::make_shared<AppConfiguration>();
    app->id_ = appConfig["Id"].asString();
    app->type_ = appConfig["Type"].asString();
    app->delay_ = appConfig["Delay"].asInt();
    app->url_ = appConfig["Url"].asString();
    
    // Default Mapping
    app->fieldMapping_.emplace("aeTitle", RemoteAET);
    app->fieldMapping_.emplace("ipAddress", RemoteIP);
    app->fieldMapping_.emplace("accessionNumber", AccessionNumber);
    app->fieldMapping_.emplace("patientId", PatientID);
    app->fieldMapping_.emplace("patientName", PatientName);
    app->fieldMapping_.emplace("gender", PatientSex);
    app->fieldMapping_.emplace("age", PatientAge);
    app->fieldMapping_.emplace("birthDate", PatientBirthDate);
    app->fieldMapping_.emplace("bodyPartExamined", BodyPartExamined);
    app->fieldMapping_.emplace("description", StudyDescription);
    app->fieldMapping_.emplace("institutionName", InstitutionName);
    app->fieldMapping_.emplace("studyDate", StudyDate);
    app->fieldMapping_.emplace("studyTime", StudyTime);
    app->fieldMapping_.emplace("studyInstanceUID", StudyInstanceUID);
    app->fieldMapping_.emplace("manufacturerModelName", ManufacturerModelName);
    app->fieldMapping_.emplace("modalityType", Modality);
    app->fieldMapping_.emplace("numOfImages", CountInstances);
    app->fieldMapping_.emplace("numOfSeries", CountSeries);
    app->fieldMapping_.emplace("operatorName", OperatorsName);
    app->fieldMapping_.emplace("referringPhysician", ReferringPhysicianName);
    app->fieldMapping_.emplace("stationName", StationName);

    app->fieldMapping_.emplace("storeId", StoreID);
    app->fieldMapping_.emplace("storeNumOfStudies", CountStudies);
    app->fieldMapping_.emplace("storeSize", TotalDiskSizeMB);
    app->fieldMapping_.emplace("publicStudyUID", PublicStudyUID);
    app->fieldMapping_.emplace("studyNumOfSeries", CountSeries);
    app->fieldMapping_.emplace("studyNumOfInstances", CountInstances);
    app->fieldMapping_.emplace("studySize", DicomDiskSizeMB);
    app->fieldMapping_.emplace("modalitiesInStudy", ModalitiesInStudy);
    app->fieldMapping_.emplace("numberOfStudyRelatedSeries", NumberOfStudyRelatedSeries);
    app->fieldMapping_.emplace("numberOfStudyRelatedInstances", NumberOfStudyRelatedInstances);
    

    if (appConfig["FieldMappingOverwrite"].asBool())
    {
      app->fieldMapping_.clear();
    }

    for (auto& m : appConfig["FieldMapping"])
    {
      std::vector<std::string> keys;
      boost::split(keys, m.asString(), boost::is_any_of(":"));
      LOG(INFO) << "Field=" << keys.size();
      if (keys.size() < 2)
      {
        LOG(ERROR) << "Invalid configuration for FieldMapping at: " << m.asString();
      }
      app->fieldMapping_.emplace(keys[0], keys[1]);
    }

    for (auto& valueMap : appConfig["FieldValues"])
    {
      for (const auto& memberName : valueMap.getMemberNames())
      {
        app->fieldValues_.emplace(memberName, valueMap[memberName.c_str()].asCString());
      }
    }

    apps_.push_back(app);
  }

}

SaolaConfiguration& SaolaConfiguration::Instance()
{
  static SaolaConfiguration configuration_;
  return configuration_;
}

std::string id_;

  bool enable_;

  std::string type_;

  unsigned int delay_ = 0;

  std::string url_;

  std::string authentication_;

  std::map<std::string, std::string> fieldMapping_;

  std::map<std::string, std::string> fieldValues_;


bool SaolaConfiguration::GetAppConfigurationById(const std::string& id, AppConfiguration& res)
{
  for (auto& app : this->apps_)
  {
    if (app->id_ == id && app->enable_)
    {
      app->clone(res);
      return true;
    }
  }
  return false;
}

int SaolaConfiguration::GetMaxRetry() const
{
  return this->maxRetry_;
}

bool SaolaConfiguration::IsEnabled() const
{
  return this->enable_;
}

const std::string& SaolaConfiguration::GetRoot() const
{
  return this->root_;
}