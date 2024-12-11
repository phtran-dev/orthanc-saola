#include "RestApi.h"

#include "SaolaConfiguration.h"
#include "SaolaDatabase.h"

#include "StableEventDTOUpdate.h"

// #include "FailedJobDTOCreate.h"
// #include "FailedJobDTOGet.h"
// #include "FailedJobFilter.h"

#include <Logging.h>

#include <boost/algorithm/string/join.hpp>

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
  Json::Value answer = Json::arrayValue;
  for (const auto& event : events)
  {
    Json::Value value;
    event.ToJson(value);
    answer.append(value);
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

  if (!requestBody.isMember("iuid") &&
      !requestBody.isMember("resource_id") &&
      !requestBody.isMember("resource_type") &&
      !requestBody.isMember("app"))
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  AppConfiguration app;
  if (!SaolaConfiguration::Instance().GetAppConfigurationById(requestBody["app"].asString(), app))
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  StableEventDTOCreate dto;
  dto.iuid_ = requestBody["iuid"].asCString();
  dto.resource_id_ = requestBody["resource_id"].asCString();
  dto.resouce_type_ = requestBody["resource_type"].asCString();
  dto.app_id_ = requestBody["app"].asCString();
  dto.delay_ = app.delay_;
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

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Get");
  }

  Json::Value answer = Json::objectValue;
  answer["result"] = SaolaDatabase::Instance().ResetEvents();
  std::string s = answer.toStyledString();

  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");  
 
}

void DeleteStableEvent(OrthancPluginRestOutput *output,
                       const char *url,
                       const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Delete)
  {
    return OrthancPluginSendMethodNotAllowed(context, output, "Delete");
  }

  std::string event_ids = request->groups[0];
  SaolaDatabase::Instance().DeleteEventByIds(event_ids);

  Json::Value answer = Json::objectValue;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}


void DeleteOrResetStableEvent(OrthancPluginRestOutput *output,
                              const char *url,
                              const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method == OrthancPluginHttpMethod_Get && request->groupsCount == 1 && std::string("reset").compare(request->groups[0]) == 0)
  {
    return ResetStableEvents(output, url, request);
  }
  else if (request->method == OrthancPluginHttpMethod_Delete)
  {
    return DeleteStableEvent(output, url, request);
  }

  return OrthancPluginSendMethodNotAllowed(context, output, "Get, Delete");
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
        SaolaDatabase::Instance().UpdateEvent(StableEventDTOUpdate(dtoGet.id_, "Lua Trigger Callback returns failure", dtoGet.retry_ + 1));
        ok = true;
      }
    }
  }

  Json::Value answer = Json::objectValue;
  answer["result"] = ok;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
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