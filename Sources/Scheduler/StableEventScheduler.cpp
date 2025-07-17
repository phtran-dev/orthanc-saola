#include "StableEventScheduler.h"
#include "../SaolaDatabase.h"
#include "../TimeUtil.h"

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include "../Cache/InMemoryJobCache.h"
#include "../DTO/TransferJobDTOCreate.h"
#include "../DTO/StableEventDTOCreate.h"
#include "../DTO/StableEventDTOUpdate.h"
#include "../DTO/MainDicomTags.h"
#include "../Config/SaolaConfiguration.h"
#include "../Constants.h"

#include "../Notification/Notification.h"

#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>

constexpr int RETENTION_EXPIRED = 3600; // 3600 secs ~ 1 hour
constexpr const char *ERROR_MESSAGE = "ErrorMessage";
constexpr const char *ERROR_DETAIL = "ErrorDetail";

static std::list<std::string> FIRST_PRIORITY_APP_TYPES = {"Ris", "StoreServer"};

static const std::string RIS_APP_TYPE = "Ris";
static const std::string STORE_SERVER_APP_TYPE = "StoreServer";

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

  if (storeStatistics.empty())
  {
    LOG(ERROR) << "[GetMainDicomTags] storeStatistics EMPTY for resourceType " << resourceType << ", resourceId " << resourceId;
  }
  else
  {
    mainDicomTags[TotalDiskSizeMB] = storeStatistics[TotalDiskSizeMB];
    mainDicomTags[CountStudies] = storeStatistics[CountStudies];
  }

  if (studyStatistics.empty())
  {
    LOG(ERROR) << "[GetMainDicomTags] studyStatistics EMPTY for resourceType " << resourceType << ", resourceId " << resourceId;
  }
  else
  {
    mainDicomTags[StudySizeMB] = studyStatistics[DicomDiskSizeMB];
  }

  if (studyMetadata.empty())
  {
    LOG(ERROR) << "[GetMainDicomTags] studyMetadata EMPTY for resourceType " << resourceType << ", resourceId " << resourceId;
    return;
  }


  // Find find the best Series
  std::set<std::string> bodyPartExamineds;
  std::set<std::string> modalitiesInStudy;
  std::string nonSRInstanceId, iid;

  mainDicomTags[Series] = Json::arrayValue;
  int countIntances = 0;

  for (const auto &seriesId : studyMetadata[Series])
  {
    Json::Value seriesMetadata;
    OrthancPlugins::RestApiGet(seriesMetadata, "/series/" + std::string(seriesId.asString()), false);
    seriesMetadata[MainDicomTags][Series_NumOfImages] = seriesMetadata[Instances].size();

    if (seriesMetadata.isMember(Instances))
    {
      countIntances += seriesMetadata[Instances].size(); 
    }

    mainDicomTags[Series].append(seriesMetadata[MainDicomTags]);

    if (seriesMetadata[MainDicomTags].isMember(BodyPartExamined))
    {
      const auto &bodyPartExamined = seriesMetadata[MainDicomTags][BodyPartExamined];
      if (!bodyPartExamined.isNull() && !bodyPartExamined.empty())
      {
        std::string str;
        Orthanc::Toolbox::ToUpperCase(str, bodyPartExamined.asString());
        if (str != "NULL")
        {
          bodyPartExamineds.insert(str);
        }
      }
    }
    std::string modality = seriesMetadata[MainDicomTags][Modality].asString();
    std::transform(modality.begin(), modality.end(), modality.begin(), ::toupper);
    modalitiesInStudy.insert(modality);
    iid = seriesMetadata[Instances][0].asString();
    if (modality != "SR")
    {
      nonSRInstanceId = seriesMetadata[Instances][0].asString();
    }
  }

  if (nonSRInstanceId.empty())
  {
    // In case there is only SR series. Make sure nonSRInstanceId is not NULL
    nonSRInstanceId = iid;
  }

  mainDicomTags[CountSeries] = studyMetadata[Series].size();
  mainDicomTags[CountInstances] = countIntances;
  mainDicomTags[NumberOfStudyRelatedSeries] = studyMetadata[Series].size();
  mainDicomTags[NumberOfStudyRelatedInstances] = countIntances;

  Json::Value instanceMetadata, instanceTags;
  OrthancPlugins::RestApiGet(instanceMetadata, "/instances/" + nonSRInstanceId + "/metadata?expand", false); // From 1.97 version
  OrthancPlugins::RestApiGet(instanceTags, "/instances/" + std::string(nonSRInstanceId) + "/simplified-tags", false);
  mainDicomTags[RemoteAET] = instanceMetadata[RemoteAET];
  mainDicomTags[RemoteIP] = instanceMetadata[RemoteIP];

  // Try to get value from embedded Itech private tags
  if (mainDicomTags[RemoteAET].isNull() || mainDicomTags[RemoteAET].empty() || mainDicomTags[RemoteAET].asString().empty() ||
      mainDicomTags[RemoteIP].isNull() || mainDicomTags[RemoteIP].empty() || mainDicomTags[RemoteIP].asString().empty())
  {
    if (mainDicomTags[RemoteAET].isNull() || mainDicomTags[RemoteAET].empty() || mainDicomTags[RemoteAET].asString().empty())
    {
      if (!instanceTags[IT_SourceApplicationEntityTitle].empty())
      {
        mainDicomTags[RemoteAET] = instanceTags[IT_SourceApplicationEntityTitle];
      }
    }
    if (mainDicomTags[RemoteIP].isNull() || mainDicomTags[RemoteIP].empty() || mainDicomTags[RemoteIP].asString().empty())
    {
      if (!instanceTags[IT_SourceIpAddress].empty())
        mainDicomTags[RemoteIP] = instanceTags[IT_SourceIpAddress];
    }
  }



  if (!bodyPartExamineds.empty())
  {
    mainDicomTags[BodyPartExamined] = boost::algorithm::join(bodyPartExamineds, ",");
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

  mainDicomTags[ModalitiesInStudy] = Json::arrayValue;
  for (const auto &modality : modalitiesInStudy)
  {
    mainDicomTags[ModalitiesInStudy].append(modality);
  }

  mainDicomTags["stable"] = true;
}

static void ConstructSeries(const Json::Value& fieldMapping, const std::string& prefix, const Json::Value& series, Json::Value& result)
{
  // get all value from appconfig where first is series_XXXXX
  for (Json::ValueConstIterator it = fieldMapping.begin(); it != fieldMapping.end(); ++it)
  {
    if (series.isMember((*it).asString()))
    {
      auto pos = (it.key().asString()).find(prefix);
      if (pos != std::string::npos)
      {
        auto key = (it.key().asString()).substr(pos + prefix.length());
        result[key] = series[(*it).asString()];
      }
    }
  }
}

static void FilterFieldMappingLevel(const std::string& prefix, const Json::Value& source, Json::Value& target)
{
  for (Json::ValueConstIterator it = source.begin(); it != source.end(); ++it)
  {
    if (it.key().asString().find(prefix) != std::string::npos)
    {
      // Found
      target[it.key().asString()] = *it;
    }
  }
}

static void ConstructAndSendMessage(const AppConfiguration &appConfig, const Json::Value &mainDicomTags)
{
  Json::Value body;
  std::string seriesPrefix = Series + std::string("_");
  for (Json::ValueConstIterator it = appConfig.fieldMapping_.begin(); it != appConfig.fieldMapping_.end(); ++it)
  {
    // Process field "Series"
    if (it.key().asString() == "series" && mainDicomTags.isMember((*it).asString()))
    {
      Json::Value newMappings = Json::objectValue;
      // Filter fields which start with "Series_"
      FilterFieldMappingLevel(seriesPrefix, appConfig.fieldMapping_, newMappings);
      body[it.key().asString()] = Json::arrayValue;
      for (auto& series : mainDicomTags[(*it).asString()])
      {
        Json::Value val;
        ConstructSeries(newMappings, seriesPrefix, series, val);
        body[it.key().asString()].append(val);
      }
      continue;
    }

    // Process fields not starting with "Series_"
    if ((it.key().asString()).find(seriesPrefix) == std::string::npos && mainDicomTags.isMember((*it).asString()))
    {
      body[it.key().asString()] = mainDicomTags[(*it).asString()];
    }
  }


  for (const auto &member : appConfig.fieldValues_.getMemberNames())
  {
    body[member] = appConfig.fieldValues_[member];
  }

  std::string bodyString;
  OrthancPlugins::WriteFastJson(bodyString, body);
  LOG(INFO) << "[ConstructAndSendMessage] Body = " << bodyString;
  OrthancPlugins::HttpClient client;
  client.SetUrl(appConfig.url_);
  client.SetTimeout(appConfig.timeOut_);
  client.SetMethod(appConfig.method_);
  client.AddHeader("Content-Type", "application/json");
  if (!appConfig.authentication_.empty())
  {
    client.AddHeader("Authorization", appConfig.authentication_);
  }
  client.SetBody(bodyString);
  client.Execute();
}

static void PrepareBody(Json::Value &body, const AppConfiguration &appConfig, const StableEventDTOGet &dto)
{
  if (appConfig.type_ == "Transfer")
  {
    body.copy(appConfig.fieldValues_);

    body["Resources"] = Json::arrayValue;
    {
      Json::Value resource;
      resource["Level"] = dto.resource_type_;
      resource["ID"] = dto.resource_id_;
      body["Resources"].append(resource);
    }
  }
  else if (appConfig.type_ == "Exporter")
  {
    body.copy(appConfig.fieldValues_);
    body["Level"] = dto.resource_type_;
    body["ID"] = dto.resource_id_;
  }
  else if (appConfig.type_ == "StoreSCU")
  {
    body.copy(appConfig.fieldValues_);
    body["Resources"] = Json::arrayValue;
    body["Resources"].append(dto.resource_id_);
  }
}

static bool ProcessAsyncTask(const AppConfiguration &appConfig, StableEventDTOGet &dto, Json::Value &notification)
{
  LOG(INFO) << "[ProcessAsyncTask] process ProcessAsyncTask: " << dto.ToJsonString();
  dto.ToJson(notification);
  try
  {
    std::list<TransferJobDTOGet> jobs;

    // Check All jobs' state. If JOB state is
    // - Success --> Delete queue and its jobs
    // - Running --> Chec if task is overdue. YES --> set try to max. NO --> wait until it finishes or being overdue
    // - Pending, Failure, Paused, Retry --> Increase queue's retry by 1 and return
    if (dto.id_ >= 0 && SaolaDatabase::Instance().GetTransferJobsByByQueueId(dto.id_, jobs))
    {
      bool jobStateOk = false;
      for (TransferJobDTOGet job : jobs)
      {
        Json::Value response;
        if (!OrthancPlugins::RestApiGet(response, "/jobs/" + job.id_, false) || response.empty())
        {
          std::stringstream ss;
          ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR Cannot call API /jobs/" << job.id_ << ", or response empty";
          LOG(ERROR) << ss.str();
          dto.failed_reason_ = ss.str();
          notification[ERROR_DETAIL] = dto.ToJsonString();
          notification[ERROR_MESSAGE] = ss.str();
          continue; // jobStateOk is still FALSE
        }

        if (!response.isMember("State"))
        {
          std::stringstream ss;
          ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR API /jobs/" << job.id_ << ", Response body does not have \"State\"";
          LOG(ERROR) << ss.str();
          dto.failed_reason_ = ss.str();
          notification[ERROR_DETAIL] = dto.ToJsonString();
          notification[ERROR_MESSAGE] = ss.str();
          continue; // jobStateOk is still FALSE
        }

        if (response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Failure) ||
            response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Paused) ||
            response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Retry))
        {
          std::stringstream ss;
          ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR API /jobs/" << job.id_ << ", Response body has State=" << response["State"].asString();
          LOG(ERROR) << ss.str();
          dto.failed_reason_ = ss.str();
          notification[ERROR_DETAIL] = dto.ToJsonString();
          notification[ERROR_MESSAGE] = ss.str();
          continue; // jobStateOk is still FALSE
        }

        if (response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Success))
        {
          LOG(INFO) << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " API /jobs/" << job.id_ << ", Response body has State=Success" << ", DELETING queue_id=" << std::to_string(dto.id_);
          SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.id_); // dto.id_ >= 0 as condition in FOR loop
          SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.id_});
          jobStateOk = true;
          break; // break loop, // jobStateOk is TRUE
        }

        if (response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Running) ||
            response["State"].asString() == Orthanc::EnumerationToString(Orthanc::JobState_Pending))
        {
          LOG(INFO) << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " API /jobs/" << job.id_ << ", Response body has State=" << response["State"].asString() << ", WAITING until it finishes. Task " << dto.id_ << " creation_time " << dto.creation_time_ << ", now " << boost::posix_time::to_iso_string(Saola::GetNow()) << ", elapsed " << Saola::Elapsed(dto.creation_time_) << " , retention " << RETENTION_EXPIRED << "(s)";
          jobStateOk = true;
          break; // break loop, jobState is now JobState_Running
        }
      }

      // If All jobs are failed then return FALSE immediately
      return jobStateOk;
    }

    Json::Value body;
    PrepareBody(body, appConfig, dto);

    LOG(INFO) << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " Send to API: " << appConfig.url_ << ", body=" << body.toStyledString();
    Json::Value jobResponse;
    if (!OrthancPlugins::RestApiPost(jobResponse, appConfig.url_, body, true))
    {
      std::stringstream ss;
      std::string s;
      OrthancPlugins::WriteFastJson(s, jobResponse);
      ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR Send to API: " << appConfig.url_ << " , Failed response=" << s;
      dto.failed_reason_ = ss.str();
      notification[ERROR_DETAIL] = dto.ToJsonString();
      notification[ERROR_MESSAGE] = ss.str();
      LOG(ERROR) << ss.str();
      return false;
    }

    // Save job
    TransferJobDTOGet result;
    if (dto.id_ >= 0)
    {
      SaolaDatabase::Instance().SaveTransferJob(TransferJobDTOCreate(jobResponse["ID"].asString(), dto.id_), result);
      LOG(INFO) << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " Save JOB " << result.ToJsonString();
    }
    return true;
  }
  catch (Orthanc::OrthancException &e)
  {
    std::stringstream ss;
    ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR Orthanc::OrthancException: " << e.What();
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
    LOG(ERROR) << ss.str();
    return false;
  }
  catch (std::exception &e)
  {
    std::stringstream ss;
    ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR std::exception: " << e.what();
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
    LOG(ERROR) << ss.str();
    return false;
  }
  catch (...)
  {
    std::stringstream ss;
    ss << "[ProcessAsyncTask][Task-" << dto.id_ << "]" << " ERROR Exception occurs but no specific reason";
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
    LOG(ERROR) << ss.str();
    return false;
  }
}

static bool ProcessSyncTask(const AppConfiguration &appConfig, StableEventDTOGet &dto, Json::Value &notification)
{
  notification["TaskType"] = appConfig.type_;
  notification["TaskContent"] = Json::objectValue;
  dto.ToJson(notification["TaskContent"]);
  try
  {
    Json::Value mainDicomTags;
    GetMainDicomTags(dto.resource_id_, Orthanc::StringToResourceType(dto.resource_type_.c_str()), mainDicomTags);
    if (!mainDicomTags.empty())
    {
      ConstructAndSendMessage(appConfig, mainDicomTags);
      if (dto.id_ >= 0)
      {
        SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.id_});
      }
      return true;
    }
    std::stringstream ss;
    ss << "[ProcessSyncTask][Task-" << dto.id_ << "]" << " ERROR Cannot get MainDicomTag";
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
  }
  catch (Orthanc::OrthancException &e)
  {
    std::stringstream ss;
    ss << "[ProcessSyncTask][Task-" << dto.id_ << "]" << " ERROR Orthanc::OrthancException: e=" << e.What();
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
  }
  catch (std::exception &e)
  {
    std::stringstream ss;
    ss << "[ProcessSyncTask][Task-" << dto.id_ << "]" << " ERROR std::exception: e=" << e.what();
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
  }
  catch (...)
  {
    std::stringstream ss;
    ss << "[ProcessSyncTask][Task-" << dto.id_ << "]" << " ERROR Exception occurs but no specific reason";
    dto.failed_reason_ = ss.str();
    notification[ERROR_DETAIL] = dto.ToJsonString();
    notification[ERROR_MESSAGE] = ss.str();
  }
  return false;
}

bool StableEventScheduler::ExecuteEvent(StableEventDTOGet &event)
{
  std::shared_ptr<AppConfiguration> appConfig = SaolaConfiguration::Instance().GetAppConfigurationById(event.app_id_);
  if (!appConfig)
  {
    LOG(ERROR) << "[ExecuteEvent] ERROR Cannot find any AppConfiguration " << event.app_id_;
    return false;
  }

  Json::Value notification;
  if (appConfig->type_ == "Transfer" || appConfig->type_ == "Exporter")
  {
    return ProcessAsyncTask(*appConfig, event, notification);
  }

  return ProcessSyncTask(*appConfig, event, notification);
}

static void MonitorTasks(std::list<StableEventDTOGet> &tasks)
{
  for (auto &task : tasks)
  {
    std::shared_ptr<AppConfiguration> appConfig = SaolaConfiguration::Instance().GetAppConfigurationById(task.app_id_);
    if (!appConfig)
    {
      LOG(ERROR) << "[MonitorTasks] ERROR Cannot find any AppConfiguration " << task.app_id_;
      SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(task.id_, "[MonitorTasks] Cannot find any AppConfiguration", SaolaConfiguration::Instance().GetMaxRetry() + 1, Saola::GetNextXSecondsFromNowInString(60).c_str()));
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(task.id_);
      continue;
    }

    if (!Saola::IsOverDue(task.last_updated_time_, task.delay_sec_))
    {
      continue;
    }
    LOG(INFO) << "[MonitorTasks] Processing task " << task.ToJsonString();

    Json::Value notification;
    notification[ERROR_MESSAGE] = "";
    if (appConfig->type_ == "Transfer" || appConfig->type_ == "Exporter" || appConfig->type_ == "StoreSCU")
    {
      if (!ProcessAsyncTask(*appConfig, task, notification))
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(task.id_, task.failed_reason_.c_str(), task.retry_ + 1, Saola::GetNextXSecondsFromNowInString(task.delay_sec_).c_str()));
        SaolaDatabase::Instance().DeleteTransferJobsByQueueId(task.id_);
        Notification::Instance().SendMessage(notification);
      }
    }
    else
    {
      if (!ProcessSyncTask(*appConfig, task, notification))
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(task.id_, task.failed_reason_.c_str(), task.retry_ + 1, Saola::GetNextXSecondsFromNowInString(task.delay_sec_).c_str()));
        Notification::Instance().SendMessage(notification);
      }
    }
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

void StableEventScheduler::Start()
{
  if (this->m_state != State_Setup)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
  }

  this->m_state = State_Running;
  
  this->m_worker1 = new boost::thread([this]()
                                    {
    while (this->m_state == State_Running)
    {
      LOG(TRACE) << "[StableEventScheduler::MonitorDatabase] Start monitoring Ris/StoreServer tasks ...";
      std::list<StableEventDTOGet> results;
      SaolaDatabase::Instance().FindByAppTypeInRetryLessThan(FIRST_PRIORITY_APP_TYPES, true, SaolaConfiguration::Instance().GetMaxRetry(), SaolaConfiguration::Instance().GetQueryLimit(), results);
      MonitorTasks(results);
      for (int i = 0; i < 10; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(SaolaConfiguration::Instance().GetThrottleDelayMs()));
      }
    } });
  this->m_worker2 = new boost::thread([this]()
                                    {
    while (this->m_state == State_Running)
    {
      LOG(TRACE) << "[StableEventScheduler::MonitorDatabase] Start monitoring Transfer/Exporter/StoreSCU tasks ...";
      std::list<StableEventDTOGet> results;
      if (SaolaConfiguration::Instance().EnableInMemJobCache())
      {
        if (InMemoryJobCache::Instance().GetSize() < SaolaConfiguration::Instance().GetInMemJobCacheLimit())
        {
          SaolaDatabase::Instance().FindByAppTypeInRetryLessThan(FIRST_PRIORITY_APP_TYPES, false, SaolaConfiguration::Instance().GetMaxRetry(), SaolaConfiguration::Instance().GetQueryLimit(), results);
        }
      }
      else
      {
        SaolaDatabase::Instance().FindByAppTypeInRetryLessThan(FIRST_PRIORITY_APP_TYPES, false, SaolaConfiguration::Instance().GetMaxRetry(), SaolaConfiguration::Instance().GetQueryLimit(), results);
      }

      MonitorTasks(results);

      for (int i = 0; i < 10; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(SaolaConfiguration::Instance().GetThrottleDelayMs()));
      }
    } });
}

void StableEventScheduler::Stop()
{
  if (this->m_state == State_Running)
  {
    this->m_state = State_Done;
    if (this->m_worker1->joinable())
    {
      this->m_worker1->join();
    }
    delete this->m_worker1;

    if (this->m_worker2->joinable())
    {
      this->m_worker2->join();
    }
    delete this->m_worker2;
  }
}
