#include "RestApi.h"
#include "../Constants.h"
#include "../Config/SaolaConfiguration.h"
#include "../SaolaDatabase.h"

#include "../DTO/StableEventDTOUpdate.h"
#include "../Scheduler/StableEventScheduler.h"

#include "../Job/ExporterJob.h"

#include "../Cache/InMemoryJobCache.h"

#include "../Database/AppConfigDatabase.h"

#include <Toolbox.h>
#include <Logging.h>
#include <MultiThreading/SharedMessageQueue.h>
#include <Compression/ZipWriter.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>


#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

static const char *const SAOLA = "Saola";

static std::string DICOM_WEB_ROOT;

namespace
{
  class SynchronousZipChunk : public Orthanc::IDynamicObject
  {
  private:
    std::string chunk_;
    bool done_;

    explicit SynchronousZipChunk(bool done) : done_(done)
    {
    }

  public:
    static SynchronousZipChunk *CreateDone()
    {
      return new SynchronousZipChunk(true);
    }

    static SynchronousZipChunk *CreateChunk(const std::string &chunk)
    {
      std::unique_ptr<SynchronousZipChunk> item(new SynchronousZipChunk(false));
      item->chunk_ = chunk;
      return item.release();
    }

    bool IsDone() const
    {
      return done_;
    }

    void SwapString(std::string &target)
    {
      if (done_)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        target.swap(chunk_);
      }
    }
  };

  class SynchronousZipStream : public Orthanc::ZipWriter::IOutputStream
  {
  private:
    boost::shared_ptr<Orthanc::SharedMessageQueue> queue_;
    uint64_t archiveSize_;

  public:
    explicit SynchronousZipStream(const boost::shared_ptr<Orthanc::SharedMessageQueue> &queue) : queue_(queue),
                                                                                                 archiveSize_(0)
    {
    }

    virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE
    {
      return archiveSize_;
    }

    virtual void Write(const std::string &chunk) ORTHANC_OVERRIDE
    {
      if (queue_.unique())
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_NetworkProtocol,
                                        "HTTP client has disconnected while creating an archive in synchronous mode");
      }
      else
      {
        queue_->Enqueue(SynchronousZipChunk::CreateChunk(chunk));
        archiveSize_ += chunk.size();
      }
    }

    virtual void Close() ORTHANC_OVERRIDE
    {
      queue_->Enqueue(SynchronousZipChunk::CreateDone());
    }
  };

}

static void GetPluginConfiguration(OrthancPluginRestOutput *output,
                                   const char *url,
                                   const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Json::Value answer;
  SaolaConfiguration::Instance().ToJson(answer);
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void GetAppConfiguration(OrthancPluginRestOutput *output,
                                const char *url,
                                const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return;
  }

  std::string appId{request->groups[0]};
  Json::Value appConfig;
  Saola::AppConfigDatabase::Instance().GetAppConfigById(appConfig, appId);

  std::string s = appConfig.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void DeleteAppConfiguration(OrthancPluginRestOutput *output,
  const char *url,
  const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  if (request->method != OrthancPluginHttpMethod_Delete)
  {
  OrthancPluginSendMethodNotAllowed(context, output, "DELETE");
  return;
  }

  std::string appId{request->groups[0]};
  SaolaConfiguration::Instance().RemoveApp(appId);

  std::string s = "{}";
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void ApplyPluginConfiguration(OrthancPluginRestOutput *output,
                                     const char *url,
                                     const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }

  Json::Value appConfigs;
  OrthancPlugins::ReadJson(appConfigs, request->body, request->bodySize);

  if (appConfigs.empty() || !appConfigs.isArray())
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  // Validate requestBody
  for (const auto &appConfig : appConfigs)
  {
    if (appConfig.empty() || !appConfig.isObject())
    {
      LOG(ERROR) << "[ApplyPluginConfiguration] ERROR Invalid request, appConfig is empty or not Object type";
      OrthancPluginSendHttpStatusCode(context, output, 400);
      return;
    }

    /*
    ** Validate FieldValues and FieldMapping
    ** Example of Configuration
      {
        "Id": "Ris1",
        "Enable": true,
        "FieldMappingOverwrite": false,
        "Type": "Ris",
        "Delay": 60, // Delay in seconds
        "Url": "http://storeserver:9001/secured/ws/rest/v1/async/location",
        "Authentication": "Basic b3J0aGFuYzpvcnRoYW5j",
        "FieldMapping": [{"aeTitle": "RemoteAET"}, {"ipAddress" : "RemoteIP"}]
      },
      {
        "Id": "Transfer1",
        "Enable": true,
        "FieldMappingOverwrite": true,
        "Type": "Transfer",
        "Delay": 200, // Delay in seconds
        "Url": "/transfers/send",
        "Method": "POST",
        "FieldValues": [{"Peer": "LongTermPeer"}, {"Compression": "none"}]
      }

    */
    for (auto key : {"FieldValues", "FieldMapping"})
    {
      if (appConfig.isMember(key))
      {
        if (appConfig[key].isNull() || appConfig[key].empty() || !appConfig[key].isArray())
        {
          LOG(ERROR) << "[ApplyPluginConfiguration] ERROR Invalid request, appConfig[\"" + std::string(key) + "\"] is null or empty or not Array type";
          OrthancPluginSendHttpStatusCode(context, output, 400);
          return;
        }

        for (const auto &value : appConfig[key])
        {
          if (!value.isObject() || value.size() != 1)
          {
            LOG(ERROR) << "[ApplyPluginConfiguration] ERROR Invalid request, values in appConfig[\"" + std::string(key) + "\"] is not (Key, Value) type: " << value.toStyledString();
            OrthancPluginSendHttpStatusCode(context, output, 400);
            return;
          }
        }
      }
    }
  }

  SaolaConfiguration::Instance().ApplyConfigurations(appConfigs, false);

  Json::Value answer;
  SaolaConfiguration::Instance().ToJson(answer);
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void GetStableEventByIds(OrthancPluginRestOutput *output,
                                const char *url,
                                const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  std::string event_ids = request->groups[0];

  // Split the string by commas
  std::vector<std::string> idStrs;
  boost::split(idStrs, request->groups[0], boost::is_any_of(","));

  std::list<int64_t> ids;
  for (const auto& val : idStrs)
  {
    ids.push_back(boost::lexical_cast<int64_t>( val.c_str()));
  }

  std::list<StableEventDTOGet> events;
  SaolaDatabase::Instance().GetByIds(ids, events);

  std::map<int64_t, Json::Value> jobMap;
  {
    std::list<TransferJobDTOGet> jobs;
    SaolaDatabase::Instance().GetTransferJobsByByQueueIds(ids, jobs);
    for (const auto& job : jobs)
    {
      Json::Value val;
      job.ToJson(val);
      auto it = jobMap.find(job.queue_id_);
      if (it == jobMap.end())
      {
        Json::Value vals = Json::arrayValue;
        vals.append(val);
        jobMap[job.queue_id_] = vals;
      }
      else
      {
        it->second.append(val);
      }
    }
  }
  

  Json::Value answer = Json::arrayValue;
  for (const auto &event : events)
  {
    Json::Value value;
    event.ToJson(value);

    auto it = jobMap.find(event.id_);
    if (it != jobMap.end())
    {
      value["jobs"] = it->second;
    }
    answer.append(value);
  }

  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void GetStableEvents(OrthancPluginRestOutput *output,
                            const char *url,
                            const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Pagination page;
  for (uint32_t i = 0; i < request->getCount; i++)
  {
    std::string key(request->getKeys[i]);
    std::string value(request->getValues[i]);
    if (key == "limit")
    {
      page.limit_ = boost::lexical_cast<unsigned int>(value);
    }
    else if (key == "offset")
    {
      page.offset_ = boost::lexical_cast<unsigned int>(value);
    }
  }

  std::list<StableEventDTOGet> events;
  SaolaDatabase::Instance().FindAll(page, events);
  Json::Value answer = Json::objectValue;
  answer["databaseIdentifier"] = SaolaConfiguration::Instance().GetDataBaseServerIdentifier();
  answer["events"] = Json::arrayValue;
  for (const auto &event : events)
  {
    Json::Value value;
    event.ToJson(value);
    answer["events"].append(value);
  }

  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void SaveStableEvent(OrthancPluginRestOutput *output,
                            const char *url,
                            const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  if (!requestBody.isMember("resource_id") &&
      !requestBody.isMember("resource_type") &&
      !requestBody.isMember("app"))
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  std::shared_ptr<AppConfiguration> app = SaolaConfiguration::Instance().GetAppConfigurationById(requestBody["app"].asString());
  if (!app)
  {
    LOG(ERROR) << "[SaveStableEvent] ERROR Cannot find any AppConfiguration " << requestBody["app"].asString();
    OrthancPluginSendHttpStatusCode(context, output, 404);
    return;
  }

  StableEventDTOCreate dto;
  dto.iuid_ = requestBody["iuid"].asCString();
  dto.resource_id_ = requestBody["resource_id"].asCString();
  dto.resouce_type_ = requestBody["resource_type"].asCString();
  dto.app_id_ = requestBody["app"].asCString();
  dto.app_type_ = app->type_.c_str();
  dto.delay_ = app->delay_;
  if (requestBody.isMember("delay"))
  {
    dto.delay_ = requestBody["delay"].asInt();
  }

  auto id = SaolaDatabase::Instance().AddEvent(dto);
  Json::Value answer = Json::objectValue;
  answer["id"] = id;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static void ExecuteStableEvents(OrthancPluginRestOutput *output,
                                const char *url,
                                const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  if (!requestBody.isMember("resource_id") &&
      !requestBody.isMember("resource_type") &&
      !requestBody.isMember("app"))
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  std::shared_ptr<AppConfiguration> app = SaolaConfiguration::Instance().GetAppConfigurationById(requestBody["app"].asString());
  if (!app)
  {
    LOG(ERROR) << "[ExecuteStableEvents] ERROR Cannot find any AppConfiguration " << requestBody["app"].asString();
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  StableEventDTOGet dto;
  dto.id_ = -1; // Not existing
  dto.iuid_ = requestBody["iuid"].asCString();
  dto.resource_id_ = requestBody["resource_id"].asCString();
  dto.resource_type_ = requestBody["resource_type"].asCString();
  dto.app_id_ = requestBody["app"].asCString();
  dto.app_type_ = app->type_.c_str();
  dto.delay_sec_ = 0;
  dto.retry_ = 0;

  if (!StableEventScheduler::Instance().ExecuteEvent(dto))
  {
    StableEventDTOCreate dto;
    dto.iuid_ = requestBody["iuid"].asCString();
    dto.resource_id_ = requestBody["resource_id"].asCString();
    dto.resouce_type_ = requestBody["resource_type"].asCString();
    dto.app_id_ = requestBody["app"].asCString();
    dto.app_type_ = app->type_.c_str();
    dto.delay_ = app->delay_;
    if (requestBody.isMember("delay"))
    {
      dto.delay_ = requestBody["delay"].asInt();
    }

    auto id = SaolaDatabase::Instance().AddEvent(dto);
    Json::Value answer = Json::objectValue;
    answer["id"] = id;
    std::string s = answer.toStyledString();
    OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
  }

  Json::Value answer = Json::objectValue;
  answer["status"] = true;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void DeleteStableEvents(OrthancPluginRestOutput *output,
                        const char *url,
                        const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Delete");
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  if (!requestBody.empty() && !requestBody.isArray())
  {
    return OrthancPluginSendHttpStatusCode(context, output, 400);
  }

  std::list<int64_t> ids;
  for (const auto &item : requestBody)
  {
    ids.push_back(item.asInt64());
  }

  SaolaDatabase::Instance().DeleteEventByIds(ids);

  Json::Value answer = Json::objectValue;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void HandleStableEvents(OrthancPluginRestOutput *output,
                        const char *url,
                        const OrthancPluginHttpRequest *request)
{
  if (request->method == OrthancPluginHttpMethod_Get)
  {
    GetStableEvents(output, url, request);
  }
  else if (request->method == OrthancPluginHttpMethod_Post)
  {
    SaveStableEvent(output, url, request);
  }
  else
  {
    return OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST, PUT, GET");
  }
}

void ResetStableEvents(OrthancPluginRestOutput *output,
                       const char *url,
                       const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  if (!requestBody.empty() && !requestBody.isArray())
  {
    return OrthancPluginSendHttpStatusCode(context, output, 400);
  }
  std::list<int64_t> ids;
  for (const auto &id : requestBody)
  {
    ids.push_back(id.asInt64());
  }

  Json::Value answer = Json::objectValue;
  answer["result"] = SaolaDatabase::Instance().ResetEvents(ids);
  std::string s = answer.toStyledString();

  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void UpdateTransferJobs(OrthancPluginRestOutput *output,
                        const char *url,
                        const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "GET");
  }

  std::string jobId = request->groups[0];
  std::string status = request->groups[1];

  LOG(INFO) << "[UpdateTransferJobs] jobId=" << jobId << ", status=" << status;

  bool ok = false;

  if (status == "success")
  {
    TransferJobDTOGet dto;
    if (SaolaDatabase::Instance().GetById(jobId, dto))
    {
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
      SaolaDatabase::Instance().DeleteEventByIds(std::list<int64_t>{dto.queue_id_});
      ok = true;
    }
    else
    {
      // TODO
    }
  }
  else if (status == "failure")
  {
    TransferJobDTOGet dto;
    if (SaolaDatabase::Instance().GetById(jobId, dto))
    {
      SaolaDatabase::Instance().DeleteTransferJobsByQueueId(dto.queue_id_);
      StableEventDTOGet dtoGet;
      if (SaolaDatabase::Instance().GetById(dto.queue_id_, dtoGet))
      {
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dtoGet.id_, "Lua Trigger Callback returns failure", dtoGet.retry_ + 1, Saola::GetNextXSecondsFromNowInString(dtoGet.delay_sec_).c_str()));
        ok = true;
      }
    }
  }

  Json::Value answer = Json::objectValue;
  answer["result"] = ok;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void ExportSingleResource(OrthancPluginRestOutput *output,
                          const char *url,
                          const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "POST");
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  std::string exportDirectory = requestBody["ExportDir"].asString();
  std::string resource = requestBody["Level"].asString();
  Orthanc::Toolbox::ToUpperCase(resource);
  std::string publicId = requestBody["ID"].asString();

  std::unique_ptr<Saola::ExporterJob> job(new Saola::ExporterJob(false, exportDirectory, resource == "STUDY" ? Orthanc::ResourceType_Study : Orthanc::ResourceType_Series));
  auto priority = 0;
  job->AddResource(publicId);
  job->SetLoaderThreads(0);
  job->SetDescription("REST API");

  boost::shared_ptr<Orthanc::SharedMessageQueue> queue(new Orthanc::SharedMessageQueue);

  const std::string &jobId = OrthancPlugins::OrthancJob::Submit(job.release(), priority);

  Json::Value answer = Json::objectValue;
  answer["ID"] = jobId;
  answer["Path"] = "/jobs/" + jobId;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void DeleteStudyResource(OrthancPluginRestOutput *output,
                         const char *url,
                         const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Put)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "PUT");
  }

  std::string studyInstanceUID = request->groups[0];

  Json::Value studyResources;
  {
    Json::Value findData;
    findData["Level"] = "Study";
    findData["Query"]["StudyInstanceUID"] = studyInstanceUID;
    findData["Expand"] = true;

    OrthancPlugins::RestApiPost(studyResources, "/tools/find", findData, false);
    if (studyResources.isNull() || studyResources.empty())
    {
      OrthancPlugins::LogInfo("[DeleteStudyResource] Cannot find StudyInstanceUID " + std::string(request->groups[0]));
      return OrthancPluginSendHttpStatusCode(context, output, 404);
    }
  }

  Json::Value requestBody;

  if (request->bodySize > 0)
  {
    // Prevent case "Cannot parse JSON: * Line 1, Column 1"
    OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);
  }

  if (requestBody.isNull() || requestBody.empty())
  {
    // Delete Studies
    bool success = true;
    for (const auto &resource : studyResources)
    {
      OrthancPlugins::LogInfo("Deleting studyInstanceUID " + studyInstanceUID + " from resource " + resource["ID"].asString());
      success &= OrthancPlugins::RestApiDelete("/studies/" + resource["ID"].asString(), false);
    }

    OrthancPlugins::LogInfo("[DeleteStudyResource] Deleted studyInstanceUID " + studyInstanceUID + (success ? " OK" : " KO"));
    return OrthancPluginSendHttpStatusCode(context, output, 200);
  }

  // Bulk delete series
  std::set<std::string> seriesInstanceUids;
  for (const auto &id : requestBody)
  {
    seriesInstanceUids.emplace(id.asString());
  }

  Json::Value deleteResourcesBody;
  for (const auto &studyResource : studyResources)
  {
    Json::Value seriesResources;
    OrthancPlugins::RestApiGet(seriesResources, "/studies/" + studyResource["ID"].asString() + "/series", false);
    for (const auto& series : seriesResources)
    {
      if (seriesInstanceUids.find(series["MainDicomTags"]["SeriesInstanceUID"].asString()) != seriesInstanceUids.end())
      {
        deleteResourcesBody["Resources"].append(series["ID"].asString());
      }
    }
  }

  if (!deleteResourcesBody.empty())
  {
    OrthancPlugins::LogInfo("Deleting seriesInstanceUIDs " + boost::join(seriesInstanceUids, ",") + " from resource " + deleteResourcesBody["Resources"].toStyledString());
    Json::Value res;
    OrthancPlugins::RestApiPost(res, "/tools/bulk-delete", deleteResourcesBody, false);
  }

  return OrthancPluginSendHttpStatusCode(context, output, 200);
}


void DeleteSeriesResource(OrthancPluginRestOutput *output,
                          const char *url,
                          const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Put)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "PUT");
  }

  std::string studyInstanceUID = request->groups[0];
  std::string seriesInstanceUID = request->groups[1];

  Json::Value seriesResource;
  {
    Json::Value findData, seriesResources;
    findData["Level"] = "Series";
    findData["Query"]["SeriesInstanceUID"] = seriesInstanceUID;
    findData["Expand"] = true;

    OrthancPlugins::RestApiPost(seriesResources, "/tools/find", findData, false);

    if (seriesResources.isNull() || seriesResources.empty())
    {
      OrthancPlugins::LogInfo("[DeleteSeriesResource] Cannot find SeriesInstanceUID " + seriesInstanceUID);
      return OrthancPluginSendHttpStatusCode(context, output, 404);
    }

    for (const auto& resource : seriesResources)
    {
      Json::Value studyResource;
      OrthancPlugins::RestApiGet(studyResource, "/studies/" + resource["ParentStudy"].asString(), false);
      if (studyResource["MainDicomTags"]["StudyInstanceUID"].asString() == studyInstanceUID)
      {
        seriesResource = resource;
        break;
      }
    }
  }

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  if (requestBody.isNull() || requestBody.empty())
  {
    // Delete Series
    OrthancPlugins::LogInfo("Deleting seriesInstanceUID " + seriesInstanceUID + " from resource " + seriesResource["ID"].asString());
    OrthancPlugins::RestApiDelete("/series/" + seriesResource["ID"].asString(), false);
    return OrthancPluginSendHttpStatusCode(context, output, 200);
  }

  // Bulk delete instances
  std::set<std::string> sopInstanceUids;
  for (const auto &id : requestBody)
  {
    sopInstanceUids.emplace(id.asString());
  }

  Json::Value instances, deleteResourcesBody;
  OrthancPlugins::RestApiGet(instances, "/series/" + seriesResource["ID"].asString() + "/instances", false);
  for (const auto& instance : instances)
  {
    if (sopInstanceUids.find(instance["MainDicomTags"]["SOPInstanceUID"].asString()) != sopInstanceUids.end())
    {
      deleteResourcesBody["Resources"].append(instance["ID"].asString());
    }
  }

  if (!deleteResourcesBody.empty())
  {
    OrthancPlugins::LogInfo("Deleting sopInstanceUIDs " + boost::join(sopInstanceUids, ",") + " from resource " + deleteResourcesBody["Resources"].toStyledString());
    Json::Value res;
    OrthancPlugins::RestApiPost(res, "/tools/bulk-delete", deleteResourcesBody, false);
  }

  return OrthancPluginSendHttpStatusCode(context, output, 200);
}


void DicomCStoreStudy(OrthancPluginRestOutput *output,
                      const char *url,
                      const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }

  if (SaolaConfiguration::Instance().EnableInMemJobCache() && InMemoryJobCache::Instance().GetSize() >= SaolaConfiguration::Instance().GetInMemJobCacheLimit())
  {
    LOG(ERROR) << "[DicomCStoreStudy] Job DicomModalityStore" << " has reached limit of " << InMemoryJobCache::Instance().GetSize()
               << " / " << SaolaConfiguration::Instance().GetInMemJobCacheLimit();
    return OrthancPluginSendHttpStatusCode(context, output, 503);
  }

  const char *modalityId = request->groups[0];
  Json::Value body;
  OrthancPlugins::ReadJson(body, request->body, request->bodySize);

  if (body.type() != Json::arrayValue)
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "The request body must contain a string");

  Json::Value resources = Json::arrayValue;
  for (const auto &val : body)
  {
    const char *resourceId = OrthancPluginLookupStudy(context, val.asCString());
    resources.append(resourceId);
  }

  Json::Value result;
  bool post = OrthancPlugins::RestApiPost(result, "/modalities/" + std::string(modalityId) + "/store", resources,
                                          false);
  if (post)
  {
    return OrthancPluginSendHttpStatusCode(context, output, 200);
  }

  return OrthancPluginSendHttpStatusCode(context, output, 500);
}

void DicomStowRsStudy(OrthancPluginRestOutput *output,
                      const char *url,
                      const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Post)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Post");
  }
  const char *server = request->groups[0];
  Json::Value body;
  OrthancPlugins::ReadJson(body, request->body, request->bodySize);

  if (body.type() != Json::arrayValue)
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "The request body must contain a string");

  Json::Value resources = Json::arrayValue;
  for (const auto &val : body)
  {
    const char *resourceId = OrthancPluginLookupStudy(context, val.asCString());
    resources.append(resourceId);
  }

  Json::Value bodyRequest = Json::objectValue;
  bodyRequest["Resources"] = resources;

  Json::Value result;
  bool post = OrthancPlugins::RestApiPost(result, DICOM_WEB_ROOT + "/servers/" + server + "/stow", bodyRequest,
                                          true);
  if (post)
  {
    return OrthancPluginSendHttpStatusCode(context, output, 200);
  }

  return OrthancPluginSendHttpStatusCode(context, output, 500);
}

void GetStudyStatistics(OrthancPluginRestOutput *output,
                        const char *url,
                        const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Json::Value findData, resourceIds;
  findData["Level"] = "Study";
  findData["Query"]["StudyInstanceUID"] = request->groups[0];

  OrthancPlugins::RestApiPost(resourceIds, "/tools/find", findData, false);

  if (resourceIds.isNull() || resourceIds.empty())
  {
    LOG(INFO) << "[GetStudyStatistics] Cannot find StudyInstanceUID " << request->groups[0];
    return OrthancPluginSendHttpStatusCode(context, output, 404);
  }

  Json::Value result;
  OrthancPlugins::RestApiGet(result, "/studies/" + resourceIds[0].asString() + "/statistics", false);
  if (!result.isNull() && !result.empty())
  {
    std::string s;
    OrthancPlugins::WriteFastJson(s, result);
    return OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
  }

  LOG(INFO) << "[GetStudyStatistics] Cannot get StudyInstanceUID " << request->groups[0] << " statistics";
  return OrthancPluginSendHttpStatusCode(context, output, 404);
}

void GetSeriesStatistics(OrthancPluginRestOutput *output,
                         const char *url,
                         const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Json::Value findData, resourceIds;
  findData["Level"] = "Series";
  findData["Query"]["SeriesInstanceUID"] = request->groups[0];

  OrthancPlugins::RestApiPost(resourceIds, "/tools/find", findData, false);

  if (resourceIds.isNull() || resourceIds.empty())
  {
    LOG(INFO) << "[SeriesInstanceUID] Cannot find StudyInstanceUID " << request->groups[0];
    return OrthancPluginSendHttpStatusCode(context, output, 404);
  }

  Json::Value result;
  OrthancPlugins::RestApiGet(result, "/series/" + resourceIds[0].asString() + "/statistics", false);
  if (!result.isNull() && !result.empty())
  {
    std::string s;
    OrthancPlugins::WriteFastJson(s, result);
    return OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
  }

  LOG(INFO) << "[SeriesInstanceUID] Cannot get SeriesInstanceUID " << request->groups[0] << " statistics";
  return OrthancPluginSendHttpStatusCode(context, output, 404);
}

void GetInMemoryJobCache(OrthancPluginRestOutput *output,
                         const char *url,
                         const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Json::Value jobs;
  InMemoryJobCache::Instance().GetJobs(jobs);

  std::string s;
  OrthancPlugins::WriteFastJson(s, jobs);
  return OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

static bool CheckSplit(OrthancPluginContext *context, const std::string &studyId)
{
  // Check if studyInstanceUID is duplicated
  {
    Json::Value study;
    OrthancPlugins::RestApiGet(study, "/studies/" + studyId, false);
    if (study.empty())
    {
      return false;
    }

    char *result;
    _OrthancPluginRetrieveDynamicString params;
    params.argument = study["MainDicomTags"]["StudyInstanceUID"].asCString();
    params.result = &result;

    OrthancPluginErrorCode err = context->InvokeService(context, _OrthancPluginService_LookupStudy, &params);
    if (err == OrthancPluginErrorCode_UnknownResource)
    {
      OrthancPlugins::LogWarning("[ITech][CheckDuplicatedStudies] Found multiple studies: " + study["MainDicomTags"]["StudyInstanceUID"].asString());
      return true;
    }
  }

  // Check if seriesInstanceUID is duplicated
  Json::Value seriesList;
  OrthancPlugins::RestApiGet(seriesList, "/studies/" + studyId + "/series", false);
  for (auto &series : seriesList)
  {
    char *result;
    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = series["MainDicomTags"]["SeriesInstanceUID"].asCString();

    OrthancPluginErrorCode err = context->InvokeService(context, _OrthancPluginService_LookupSeries, &params);

    if (err == OrthancPluginErrorCode_UnknownResource)
    {
      OrthancPlugins::LogWarning("[ITech][CheckDuplicatedSeries] Found multiple series: " + series["MainDicomTags"]["SeriesInstanceUID"].asString());
      return true;
    }
  }
  // Check if sopInstanceUID is duplicated
  Json::Value instanceList;
  OrthancPlugins::RestApiGet(instanceList, "/studies/" + studyId + "/instances", false);
  for (auto &instance : instanceList)
  {
    char *result;
    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = instance["MainDicomTags"]["SOPInstanceUID"].asCString();
    OrthancPluginErrorCode err = context->InvokeService(context, _OrthancPluginService_LookupInstance, &params);
    if (err == OrthancPluginErrorCode_UnknownResource)
    {
      OrthancPlugins::LogWarning("[ITech][CheckDuplicatedInstances] Found multiple instances: " + instance["MainDicomTags"]["SOPInstanceUID"].asString());
      return true;
    }
  }
  return false;
}

static void CheckStudyDuplicated(OrthancPluginRestOutput *output,
                                 const char *url,
                                 const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return;
  }

  std::string studyId{request->groups[0]};
  Json::Value answer = Json::objectValue;
  answer["result"] = CheckSplit(context, studyId);
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void SplitStudy(OrthancPluginRestOutput *output,
                const char *url,
                const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
    return;
  }

  std::string studyId{request->groups[0]};

  Json::Value instances;
  OrthancPlugins::RestApiGet(instances, "/studies/" + studyId + "/instances", false);
  std::string instanceId = instances[0]["ID"].asCString();
  Json::Value instanceMetadata;
  OrthancPlugins::RestApiGet(instanceMetadata, "/instances/" + instanceId + "/metadata?expand", false);

  // Split study
  Json::Value body = Json::objectValue;
  body["Replace"] = Json::objectValue;
  body["Replace"][IT_TAG_PrivateCreator] = IT_VAL_PrivateCreator;
  body["Replace"][IT_TAG_SourceIpAddress] = instanceMetadata["RemoteIP"];
  body["Replace"][IT_TAG_SourceApplicationEntityTitle] = instanceMetadata["RemoteAET"];
  body["PrivateCreator"] = IT_VAL_PrivateCreator;

  Json::Value answer;
  OrthancPlugins::RestApiPost(answer, "/studies/" + studyId + "/modify", body, false);

  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void RegisterRestEndpoint()
{
  OrthancPlugins::RegisterRestCallback<GetAppConfiguration>(SaolaConfiguration::Instance().GetRoot() + ORTHANC_PLUGIN_NAME + "/configuration/apps/([^/]*)", true);
  OrthancPlugins::RegisterRestCallback<DeleteAppConfiguration>(SaolaConfiguration::Instance().GetRoot() + ORTHANC_PLUGIN_NAME + "/configuration/delete-apps/([^/]*)", true);

  OrthancPlugins::RegisterRestCallback<GetPluginConfiguration>(SaolaConfiguration::Instance().GetRoot() + ORTHANC_PLUGIN_NAME + "/configuration", true);
  OrthancPlugins::RegisterRestCallback<ApplyPluginConfiguration>(SaolaConfiguration::Instance().GetRoot() + ORTHANC_PLUGIN_NAME + "/configuration/apply", true);
  OrthancPlugins::RegisterRestCallback<HandleStableEvents>(SaolaConfiguration::Instance().GetRoot() + "event-queues", true);
  OrthancPlugins::RegisterRestCallback<DeleteStableEvents>(SaolaConfiguration::Instance().GetRoot() + "delete-event-queues", true);
  OrthancPlugins::RegisterRestCallback<ResetStableEvents>(SaolaConfiguration::Instance().GetRoot() + "reset-event-queues", true);
  OrthancPlugins::RegisterRestCallback<ExecuteStableEvents>(SaolaConfiguration::Instance().GetRoot() + "execute-event-queues", true);
  OrthancPlugins::RegisterRestCallback<GetStableEventByIds>(SaolaConfiguration::Instance().GetRoot() + "event-queues/([^/]*)", true);
  OrthancPlugins::RegisterRestCallback<UpdateTransferJobs>(SaolaConfiguration::Instance().GetRoot() + "transfer-jobs/([^/]*)/([^/]*)", true);
  OrthancPlugins::RegisterRestCallback<ExportSingleResource>(SaolaConfiguration::Instance().GetRoot() + "export", true);
  OrthancPlugins::RegisterRestCallback<DeleteStudyResource>(SaolaConfiguration::Instance().GetRoot() + "studies/([^/]*)/delete", true);    // For compatibility
  OrthancPlugins::RegisterRestCallback<DeleteSeriesResource>(SaolaConfiguration::Instance().GetRoot() + "studies/([^/]*)/series/([^/]*)/delete", true);
  OrthancPlugins::RegisterRestCallback<DicomCStoreStudy>(SaolaConfiguration::Instance().GetRoot() + "modalities/([^/]*)/store", true);     // For compatibility
  OrthancPlugins::RegisterRestCallback<GetStudyStatistics>(SaolaConfiguration::Instance().GetRoot() + "studies/([^/]*)/statistics", true); // For compatibility
  OrthancPlugins::RegisterRestCallback<GetSeriesStatistics>(SaolaConfiguration::Instance().GetRoot() + "series/([^/]*)/statistics", true); // For compatibility
  OrthancPlugins::RegisterRestCallback<GetInMemoryJobCache>(SaolaConfiguration::Instance().GetRoot() + "jobcache", true);                  // For compatibility

  OrthancPlugins::OrthancConfiguration dicomWebConfiguration;
  {
    OrthancPlugins::OrthancConfiguration globalConfiguration;
    globalConfiguration.GetSection(dicomWebConfiguration, "DicomWeb");
  }

  if (dicomWebConfiguration.GetBooleanValue("Enable", false))
  {
    DICOM_WEB_ROOT = dicomWebConfiguration.GetStringValue("Root", "/wado-rs/");
    if (DICOM_WEB_ROOT.back() == '/')
    {
      DICOM_WEB_ROOT.pop_back();
    }
    OrthancPlugins::RegisterRestCallback<DicomStowRsStudy>(DICOM_WEB_ROOT + SaolaConfiguration::Instance().GetRoot() + "servers/([^/]*)/stow", true);
  }

  OrthancPlugins::RegisterRestCallback<SplitStudy>(SaolaConfiguration::Instance().GetRoot() + "studies/([^/]*)/split", true);                   // For compatibility
  OrthancPlugins::RegisterRestCallback<CheckStudyDuplicated>(SaolaConfiguration::Instance().GetRoot() + "studies/([^/]*)/is-duplicated", true); // For compatibility
}

// void ResetFailedJobs(OrthancPluginRestOutput *output,
//                      const char *url,
//                      const OrthancPluginHttpRequest *request)
// {
//   OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

//   Json::Value requestBody;
//   OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

//   std::list<std::string> ids;
//   if (!requestBody.isNull() && !requestBody.empty())
//   {
//     for (const auto& id : requestBody)
//     {
//       ids.push_back(id.asString());
//     }
//   }

//   Json::Value answer = Json::objectValue;
//   answer["result"] = SaolaDatabase::Instance().ResetFailedJob(ids);
//   std::string s = answer.toStyledString();
//   OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
// }

// void SaveFailedJob(OrthancPluginRestOutput *output,
//                    const char *url,
//                    const OrthancPluginHttpRequest *request)
// {
//   OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

//   Json::Value requestJson;
//   OrthancPlugins::ReadJson(requestJson, request->body, request->bodySize);

//   FailedJobDTOCreate dto = FailedJobDTOCreate();
//   dto.id_ = requestJson["id"].asCString();
//   OrthancPlugins::WriteFastJson(dto.content_, requestJson["content"]);

//   FailedJobDTOGet result;
//   SaolaDatabase::Instance().SaveFailedJob(dto, result);

//   Json::Value answer = Json::objectValue;
//   result.ToJson(answer);
//   std::string s = answer.toStyledString();
//   OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
// }

// void GetFailedJobs(OrthancPluginRestOutput *output,
//                    const char *url,
//                    const OrthancPluginHttpRequest *request)
// {
//   OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

//   Pagination page;
//   FailedJobFilter filter(-1, -1);

//   for (uint32_t i = 0; i < request->getCount; i++)
//   {
//     std::string key(request->getKeys[i]);
//     std::string value(request->getValues[i]);
//     if (key == "limit")
//     {
//       page.limit_ = boost::lexical_cast<unsigned int>(value);
//     }
//     else if (key == "offset")
//     {
//       page.offset_ = boost::lexical_cast<unsigned int>(value);
//     }
//     else if (key== "retry")
//     {
//       if (value.size() > 2)
//       {
//         std::string cmp(&value[0], &value[2]);
//         std::string number(&value[2]);
//         if (cmp == "lt")
//         {
//           filter.max_retry_ = boost::lexical_cast<int>(number);
//         }
//         else if (cmp == "gt")
//         {
//           filter.min_retry_ = boost::lexical_cast<int>(number);
//         }
//       }
//     }
//   }

//   std::list<FailedJobDTOGet> jobs;
//   SaolaDatabase::Instance().FindAll(page, filter, jobs);
//   Json::Value answer = Json::arrayValue;
//   for (const auto& job : jobs)
//   {
//     Json::Value value;
//     job.ToJson(value);
//     answer.append(value);
//   }

//   std::string s = answer.toStyledString();
//   OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
// }

// void DeleteFailedJob(OrthancPluginRestOutput *output,
//                      const char *url,
//                      const OrthancPluginHttpRequest *request)
// {
//   OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

//   if (request->method != OrthancPluginHttpMethod_Delete)
//   {
//     return OrthancPluginSendMethodNotAllowed(context, output, "Delete");
//   }

//   std::string ids = request->groups[0];
//   SaolaDatabase::Instance().DeleteFailedJobByIds(std::list<std::string>{ids});

//   Json::Value answer = Json::objectValue;
//   std::string s = answer.toStyledString();
//   OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
// }

// void HandleFailedJobService(OrthancPluginRestOutput *output,
//                             const char *url,
//                             const OrthancPluginHttpRequest *request)
// {
//   if (request->method == OrthancPluginHttpMethod_Post)
//   {
//     SaveFailedJob(output, url, request);
//   }
//   else if (request->method == OrthancPluginHttpMethod_Put)
//   {
//     ResetFailedJobs(output, url, request);
//   }
//   else if (request->method == OrthancPluginHttpMethod_Get)
//   {
//     GetFailedJobs(output, url, request);
//   }
//   else
//   {
//     return OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST, PUT, GET");
//   }
// }

// void RetryFailedJobs(OrthancPluginRestOutput *output,
//                      const char *url,
//                      const OrthancPluginHttpRequest *request)
// {

//   std::string id = request->groups[0];
//   FailedJobDTOGet job;
//   if (!SaolaDatabase::Instance().GetById(id, job))
//   {
//     OrthancPluginSendHttpStatusCode(OrthancPlugins::GetGlobalContext(), output, 404);
//   }

//   Json::Value response;
//   bool ok = OrthancPlugins::RestApiGet(response, "/jobs/" + job.id_, false) && !response.empty();
//   if (ok)
//   {
//     // Resubmit job
//     if (response["State"].asString() == "Failure")
//     {
//       Json::Value result;
//       ok = OrthancPlugins::RestApiPost(result, "/jobs/" + job.id_ + "/resubmit", std::string(""), false);
//     }
//   }
//   else
//   {
//     // Submit a new job
//     try
//     {
//       Json::Value json;
//       LOG(INFO) << "[RetryFailedJobs] job content=" << job.content_;
//       OrthancPlugins::ReadJson(json, job.content_);
//       LOG(INFO) << "[RetryFailedJobs] After parse = " << json.toStyledString();
//       if (!job.content_.empty() && OrthancPlugins::ReadJson(json, job.content_) && !json.empty() && json.isMember("Content")
//       && json.isMember("Type") && json["Type"].asString() == "PushTransfer")
//       {
//         LOG(INFO) << "[RetryFailedJobs] PushTransfer Again";
//         Json::Value body = json["Content"];
//         Json::Value result;
//         if (OrthancPlugins::RestApiPost(result, "/push/transfer", body, false))
//         {
//           // Delete Old Job
//           ok = SaolaDatabase::Instance().DeleteFailedJobByIds(std::list<std::string>{job.id_});
//         }
//         else
//         {
//           ok = false;
//         }
//       }
//       else
//       {
//         ok = false;
//       }
//     }
//     catch (std::exception& e)
//     {
//       LOG(INFO) << "[RetryFailedJobs] Parsing json:" << job.content_ << "Caught exception: " << e.what();
//       ok = false;
//     }
//   }

//   if (!ok)
//   {
//     // Increase retry
//     FailedJobDTOCreate dto;
//     dto.id_ = job.id_;
//     dto.content_ = job.content_;

//     FailedJobDTOGet result;
//     SaolaDatabase::Instance().SaveFailedJob(dto, result);
//   }

//   Json::Value answer = Json::objectValue;
//   answer["result"] = ok;
//   std::string s = answer.toStyledString();
//   OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, s.c_str(), s.size(), "application/json");
// }
