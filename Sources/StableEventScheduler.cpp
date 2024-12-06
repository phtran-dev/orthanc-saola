#include "StableEventScheduler.h"
#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include "StableEventDTOCreate.h"
#include "StableEventDTOUpdate.h"
#include "MainDicomTags.h"
#include "SaolaConfiguration.h"
#include "Constants.h"

#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>

static void GetMainDicomTags(const std::string& resourceId, const Orthanc::ResourceType& resourceType, Json::Value& mainDicomTags)
{
  Json::Value storeStatistics, studyStatistics, studyMetadata;
  
  switch (resourceType)
  {
    case Orthanc::ResourceType_Study:
      if (!OrthancPlugins::RestApiGet(studyMetadata, "/studies/" + resourceId, false))
      {
        return;
      }
      break;

    case Orthanc::ResourceType_Series :
      if (!OrthancPlugins::RestApiGet(studyMetadata, "/series/" + resourceId + "/study", false))
      {
        return;
      }
      break;
  
    case Orthanc::ResourceType_Instance:
      if (!OrthancPlugins::RestApiGet(studyMetadata, "/instances/" + resourceId + "/study", false))
      {
        return;
      }
      break;
    default:
      return;
  }

  if (studyMetadata.empty() || !studyMetadata.isMember("ID"))
  {
    return;
  }

  std::string studyId = studyMetadata["ID"].asCString();
  
  OrthancPlugins::RestApiGet(storeStatistics, "/statistics", false);
  OrthancPlugins::RestApiGet(studyStatistics, "/studies/" + studyId + "/statistics", false);
  OrthancPlugins::RestApiGet(studyMetadata, "/studies/" + studyId, false);

  if (storeStatistics.empty() || studyStatistics.empty() || studyMetadata.empty())
  {
    return;
  }

  // Find find the best Series
  std::set<std::string> bodyPartExamineds;
  std::set<std::string> modalitiesInStudy;
  std::string instanceId;
  
  for (const auto &seriesId : studyMetadata["Series"])
  {
    Json::Value seriesMetadata;
    OrthancPlugins::RestApiGet(seriesMetadata, "/series/" + std::string(seriesId.asString()), false);

    if (seriesMetadata["MainDicomTags"].isMember("BodyPartExamined"))
    {
      const auto &bodyPartExamined = seriesMetadata["MainDicomTags"]["BodyPartExamined"];
      if (!bodyPartExamined.isNull() && !bodyPartExamined.empty())
      {
        const std::string &str = bodyPartExamined.asString();
        if (str != "Null")
        {
          bodyPartExamineds.insert(str);
        }
      }
    }
    std::string modality = seriesMetadata["MainDicomTags"]["Modality"].asString();
    std::transform(modality.begin(), modality.end(), modality.begin(), ::toupper);
    modalitiesInStudy.insert(modality);
    if (modality != "SR")
    {
      instanceId = seriesMetadata["Instances"][0].asString();
    } 
  }

  Json::Value instanceMetadata, instanceTags;
  OrthancPlugins::RestApiGet(instanceMetadata, "/instances/" + instanceId + "/metadata?expand", false); // From 1.97 version
  OrthancPlugins::RestApiGet(instanceTags, "/instances/" + std::string(instanceId) + "/simplified-tags", false);
  mainDicomTags["RemoteAET"] = instanceMetadata["RemoteAET"];
  mainDicomTags["RemoteIP"] = instanceMetadata["RemoteIP"];

  // Try to get value from embedded Itech private tags
    if (mainDicomTags["RemoteAET"].isNull() || mainDicomTags["RemoteAET"].empty() || mainDicomTags["RemoteAET"].asString().empty() ||
        mainDicomTags["RemoteIP"].isNull() || mainDicomTags["RemoteIP"].empty() || mainDicomTags["RemoteIP"].asString().empty())
    {
      if (mainDicomTags["RemoteAET"].isNull() || mainDicomTags["RemoteAET"].empty() || mainDicomTags["RemoteAET"].asString().empty())
      {
          if (!instanceTags[IT_SourceApplicationEntityTitle].empty())
              mainDicomTags["aeTitle"] = instanceTags[IT_SourceApplicationEntityTitle];
      }
      if (mainDicomTags["RemoteIP"].isNull() || mainDicomTags["RemoteIP"].empty() || mainDicomTags["RemoteAET"].asString().empty())
      {
          if (!instanceTags[IT_SourceIpAddress].empty())
              mainDicomTags["RemoteIP"] = instanceTags[IT_SourceIpAddress];
      }
    }

  mainDicomTags["CountSeries"] = studyStatistics["CountSeries"];
  mainDicomTags["CountInstances"] = studyStatistics["CountInstances"];
  if (!bodyPartExamineds.empty())
  {
    mainDicomTags["BodyPartExamined"] = boost::algorithm::join(bodyPartExamineds, ",");
  }
  mainDicomTags[AccessionNumber] = instanceTags[AccessionNumber];
  mainDicomTags[StudyInstanceUID] = instanceTags[StudyInstanceUID];
  mainDicomTags[PublicStudyUID] = studyMetadata[PublicStudyUID];
  mainDicomTags[StudyTime] = instanceTags[StudyTime];
  mainDicomTags[StudyDate] = instanceTags[StudyDate];
  mainDicomTags[ReferringPhysicianName] = instanceTags[ReferringPhysicianName];
  mainDicomTags[InstitutionName] = instanceTags[InstitutionName];
  mainDicomTags[StationName] = instanceTags[StationName];
  mainDicomTags[ManufacturerModelName] = instanceTags[ManufacturerModelName];
  mainDicomTags[StudyDescription] = instanceTags[StudyDescription];
  mainDicomTags[Modality] = instanceTags[Modality];
  mainDicomTags[OperatorsName] = instanceTags[OperatorsName];
  mainDicomTags[PatientSex] = instanceTags[PatientSex];
  if (mainDicomTags[PatientSex].isNull() || mainDicomTags[PatientSex].asString().empty())
  {
    mainDicomTags[PatientSex] = "O";
  }
  mainDicomTags[PatientBirthDate] = instanceTags[PatientBirthDate];
  mainDicomTags[PatientID] = instanceTags[PatientID];
  mainDicomTags[PatientName] = instanceTags[PatientName];
  mainDicomTags[PatientAge] = instanceTags[PatientAge];

  mainDicomTags[TotalDiskSizeMB] = storeStatistics[TotalDiskSizeMB];
  mainDicomTags[CountStudies] = storeStatistics[CountStudies];
  mainDicomTags[CountSeries] = studyStatistics[CountSeries];
  mainDicomTags[CountInstances] = studyStatistics[CountInstances];
  mainDicomTags[StudySizeMB] = studyStatistics[DicomDiskSizeMB];

  mainDicomTags[ModalitiesInStudy] = Json::arrayValue;
  for (const auto& modality : modalitiesInStudy)
  {
    mainDicomTags[ModalitiesInStudy].append(modality);
  }

  mainDicomTags[NumberOfStudyRelatedSeries] = studyStatistics[CountSeries];
  mainDicomTags[NumberOfStudyRelatedInstances] = studyStatistics[CountInstances];
  mainDicomTags["stable"] = true;
}


static void ConstructAndSendMessage(const std::string& appType, const Json::Value& mainDicomTags)
{
  std::list<std::shared_ptr<AppConfiguration>> apps;
  SaolaConfiguration::Instance().GetAppConfiguration(appType, apps);

  for (const auto& app : apps)
  {
    Json::Value body;
    for (auto& field : app->fieldMapping_)
    {
      if (mainDicomTags.isMember(field.second))
      {
        body[field.first] = mainDicomTags[field.second];
      }
    }

    for (auto& field : app->fieldValues_)
    {
      body[field.first] = field.second;
    }

    std::string bodyString;
    OrthancPlugins::WriteFastJson(bodyString, body);

    LOG(INFO) << "[ConstructAndSendMessage] Body = " << body.toStyledString();
    OrthancPlugins::HttpClient client;
    client.SetUrl(app->url);
    client.SetTimeout(5);
    if (!app->authentication.empty())
    {
      client.AddHeader("Authentication", app->authentication);
    }
    client.SetBody(bodyString);
    client.Execute();
  }
}


StableEventScheduler& StableEventScheduler::Instance()
{
    static StableEventScheduler instance;
    return instance;
}

StableEventScheduler::~StableEventScheduler()
{
  if (this->m_state == State_Running)
  {
      OrthancPlugins::LogError("StableEventScheduler::Stop() should have been manually called");
      Stop();
  }
}

void StableEventScheduler::MonitorDatabase()
{
  LOG(INFO) << "[MonitorDatabase] Start monitoring ...";
  std::list<StableEventDTOGet> results;
  SaolaDatabase::Instance().FindByRetryLessThan(5, results);
  for (const auto& a : results)
  {
    LOG(INFO) << "[MonitorDatabase] Result {id=" << a.id_ << ", iuid=" << a.iuid_ << ", resource_id=" << a.resource_id_ << ", resource_type=" << a.resource_type_ <<
    ", creation_time=" << a.creation_time_<< "}";

    boost::posix_time::ptime t = boost::posix_time::from_iso_string(a.creation_time_);

    LOG(INFO) << "[MonitorDatabase] time elapsed: " << GetNow() - t;

    if (GetNow() - t > boost::posix_time::seconds(a.delay_sec_))
    {
      try
      {
        Json::Value mainDicomTags;
        GetMainDicomTags(a.resource_id_, Orthanc::StringToResourceType(a.resource_type_.c_str()), mainDicomTags);
        if (!mainDicomTags.empty())
        {
          ConstructAndSendMessage(a.app_, mainDicomTags);
        }
        else
        {
          SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(a.id_, "Cannnot get MainDicomTag", a.retry_ + 1));
        }
      }
      catch (Orthanc::OrthancException& e)
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(a.id_, e.What(), a.retry_ + 1));
      }
      catch (...)
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(a.id_, "No reason found", a.retry_ + 1));
      }
    }
  }
}


void StableEventScheduler::Start()
{
    if (this->m_state != State_Setup)
    {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }

    this->m_state = State_Running;

    const auto intervalSeconds = 10;

    this->m_worker = new std::thread([this, intervalSeconds]() {
      while (this->m_state == State_Running)
      {
        this->MonitorDatabase();
        for (unsigned int i = 0; i < intervalSeconds * 10; i++)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
    });
}

void StableEventScheduler::Stop()
{
    if (this->m_state == State_Running)
    {
        this->m_state = State_Done;
        if (this->m_worker->joinable())
            this->m_worker->join();
        delete this->m_worker;
    }
}

    


