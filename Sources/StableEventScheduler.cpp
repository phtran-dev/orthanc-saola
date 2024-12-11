#include "StableEventScheduler.h"
#include "SaolaDatabase.h"
#include "TimeUtil.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include "TransferJobDTOCreate.h"
#include "StableEventDTOCreate.h"
#include "StableEventDTOUpdate.h"
#include "MainDicomTags.h"
#include "SaolaConfiguration.h"
#include "Constants.h"

#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>

static void GetMainDicomTags(const std::string &resourceId, const Orthanc::ResourceType &resourceType, Json::Value &mainDicomTags)
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

  case Orthanc::ResourceType_Series:
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
  for (const auto &modality : modalitiesInStudy)
  {
    mainDicomTags[ModalitiesInStudy].append(modality);
  }

  mainDicomTags[NumberOfStudyRelatedSeries] = studyStatistics[CountSeries];
  mainDicomTags[NumberOfStudyRelatedInstances] = studyStatistics[CountInstances];
  mainDicomTags["stable"] = true;
}

static void ConstructAndSendMessage(const AppConfiguration &appConfig, const Json::Value &mainDicomTags)
{
  Json::Value body;
  for (auto &field : appConfig.fieldMapping_)
  {
    if (mainDicomTags.isMember(field.second))
    {
      body[field.first] = mainDicomTags[field.second];
    }
  }

  for (const auto& member : appConfig.fieldValues_.getMemberNames())
  {
    body[member] = appConfig.fieldValues_[member];
  }

  std::string bodyString;
  OrthancPlugins::WriteFastJson(bodyString, body);
  LOG(INFO) << "[ConstructAndSendMessage] Body = " << body.toStyledString();
  OrthancPlugins::HttpClient client;
  client.SetUrl(appConfig.url_);
  client.SetMethod(appConfig.method_);
  client.AddHeader("Content-Type", "application/json");
  if (!appConfig.authentication_.empty())
  {
    client.AddHeader("Authorization", appConfig.authentication_);
  }
  client.SetBody(bodyString);
  client.Execute();
}

static void ProcessTransferTask(const AppConfiguration &appConfig, const StableEventDTOGet &dto)
{
  LOG(INFO) << "[ProcessTransferTask] process ProcessTransferTask id=" << dto.id_;
  try
  {
    std::list<TransferJobDTOGet> jobs;

    // Check if still running
    if (SaolaDatabase::Instance().GetTransferJobsByByQueueId(dto.id_, jobs))
    {
      for (TransferJobDTOGet job : jobs)
      {
        Json::Value response;
        if (OrthancPlugins::RestApiGet(response, "/jobs/" + job.id_, false) && !response.empty())
        {
          if (response.isMember("State") &&
              (response["State"].asString() == "Pending" ||
               response["State"].asString() == "Running" ||
               response["State"].asString() == "Success"))
          {
            LOG(INFO) << "[ProcessTransferTask] Do not process Task id=" << dto.id_ << " which has state=" << response.toStyledString();
            return;
          }
        }
      }
    }

    Json::Value body;
    body.copy(appConfig.fieldValues_);

    body["Resources"] = Json::arrayValue;
    {
      Json::Value resource;
      resource["Level"] = dto.resource_type_;
      resource["ID"] = dto.resource_id_;
      body["Resources"].append(resource);
    }

    LOG(INFO) << "[ProcessTransferTask] body=" << body.toStyledString() << ", url=" << appConfig.url_;
    Json::Value jobResponse;
    if (!OrthancPlugins::RestApiPost(jobResponse, appConfig.url_, body, true))
    {
      LOG(INFO) << "[ProcessTransferTask] response=" << jobResponse.toStyledString();
      SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, "Cannot POST to TRANSFER PLUGIN", dto.retry_ + 1));
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.id_);
      return;
    }

    // Save job
    TransferJobDTOGet result;
    SaolaDatabase::Instance().SaveTransferJob(TransferJobDTOCreate(jobResponse["ID"].asString(), dto.id_), result);
  }
  catch (Orthanc::OrthancException &e)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, e.What(), dto.retry_ + 1));
    SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.id_);
  }
  catch (std::exception &e)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, e.what(), dto.retry_ + 1));
    SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.id_);
  }
  catch (...)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, "Exception occurs but no specific reason", dto.retry_ + 1));
    SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.id_);
  }
}

static void ProcessExternalTask(const AppConfiguration &appConfig, const StableEventDTOGet &dto)
{
  try
  {
    Json::Value mainDicomTags;
    GetMainDicomTags(dto.resource_id_, Orthanc::StringToResourceType(dto.resource_type_.c_str()), mainDicomTags);
    if (!mainDicomTags.empty())
    {
      ConstructAndSendMessage(appConfig, mainDicomTags);
      SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.id_});
    }
    else
    {
      SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, "Cannnot get MainDicomTag", dto.retry_ + 1));
    }
  }
  catch (std::exception &e)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, e.what(), dto.retry_ + 1));
  }
  catch (Orthanc::OrthancException &e)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, e.What(), dto.retry_ + 1));
  }
  catch (...)
  {
    SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dto.id_, "Exception with no reason found", dto.retry_ + 1));
  }
}

StableEventScheduler &StableEventScheduler::Instance()
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
  LOG(INFO) << "[StableEventScheduler::MonitorDatabase] Start monitoring ...";
  std::list<StableEventDTOGet> results;
  SaolaDatabase::Instance().FindByRetryLessThan(SaolaConfiguration::Instance().GetMaxRetry(), results);
  for (const auto &a : results)
  {
    LOG(INFO) << "[MonitorDatabase] Result {id=" << a.id_ << ", iuid=" << a.iuid_ << ", resource_id=" << a.resource_id_ << ", resource_type=" << a.resource_type_ << ", creation_time=" << a.creation_time_ << "}";

    boost::posix_time::ptime t = boost::posix_time::from_iso_string(a.creation_time_);

    LOG(INFO) << "[MonitorDatabase] time elapsed: " << GetNow() - t << ", delay=" << a.delay_sec_;

    if (GetNow() - t < boost::posix_time::seconds(a.delay_sec_))
    {
      continue;
    }

    AppConfiguration appConfig;
    SaolaConfiguration::Instance().GetAppConfigurationById(a.app_id_, appConfig);
    if (appConfig.type_ == "Transfer")
    {
      ProcessTransferTask(appConfig, a);
    }
    else
    {
      ProcessExternalTask(appConfig, a);
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
  this->m_worker = new std::thread([this, intervalSeconds]()
                                   {
    while (this->m_state == State_Running)
    {
      this->MonitorDatabase();
      for (unsigned int i = 0; i < intervalSeconds * 10; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    } });
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
