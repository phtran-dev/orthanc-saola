#include "RestApi.h"

#include "SaolaDatabase.h"

#include "FailedJobDTOCreate.h"
#include "FailedJobDTOGet.h"
#include "FailedJobFilter.h"

#include <Logging.h>

#include <boost/algorithm/string/join.hpp>

void GetStableEvents(OrthancPluginRestOutput *output,
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

void SaveStableEvent(OrthancPluginRestOutput *output,
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

  if (requestBody["app"].asString() != "Ris" &&
      requestBody["app"].asString() != "StoreServer")
  {
    OrthancPluginSendHttpStatusCode(context, output, 400);
    return;
  }

  StableEventDTOCreate dto;
  dto.iuid_ = requestBody["iuid"].asCString();
  dto.resource_id_ = requestBody["resource_id"].asCString();
  dto.resouce_type_ = requestBody["resource_type"].asCString();
  dto.app_ = requestBody["app"].asCString();

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


void ResetFailedJob(OrthancPluginRestOutput *output,
                    const char *url,
                    const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();
  

  Json::Value requestBody;
  OrthancPlugins::ReadJson(requestBody, request->body, request->bodySize);

  std::list<std::string> ids;
  for (const auto& id : requestBody)
  {
    ids.push_back(id.asString());
  }

  Json::Value answer = Json::objectValue;
  answer["result"] = SaolaDatabase::Instance().ResetFailedJob(ids);
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}


void SaveFailedJob(OrthancPluginRestOutput *output,
                   const char *url,
                   const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  Json::Value requestJson;
  OrthancPlugins::ReadJson(requestJson, request->body, request->bodySize);

  FailedJobDTOCreate dto = FailedJobDTOCreate();
  dto.id_ = requestJson["id"].asCString();
  OrthancPlugins::WriteFastJson(dto.content_, requestJson);

  FailedJobDTOGet result;
  SaolaDatabase::Instance().SaveFailedJob(dto, result);

  Json::Value answer = Json::objectValue;
  result.ToJson(answer);
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void GetFailedJobs(OrthancPluginRestOutput *output,
                   const char *url,
                   const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  Pagination page;
  FailedJobFilter filter(-1, -1);

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
    else if (key== "retry")
    {
      if (value.size() > 2)
      {
        std::string cmp(&value[0], &value[2]);
        std::string number(&value[2]);
        if (cmp == "lt")
        {
          filter.max_retry_ = boost::lexical_cast<int>(number);
        }
        else if (cmp == "gt")
        {
          filter.min_retry_ = boost::lexical_cast<int>(number);
        }
      }
    }
  }

  std::list<FailedJobDTOGet> jobs;
  SaolaDatabase::Instance().FindAll(page, filter, jobs);
  Json::Value answer = Json::arrayValue;
  for (const auto& job : jobs)
  {
    Json::Value value;
    job.ToJson(value);
    answer.append(value);
  }

  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void DeleteFailedJob(OrthancPluginRestOutput *output,
                     const char *url,
                     const OrthancPluginHttpRequest *request)
{
  OrthancPluginContext *context = OrthancPlugins::GetGlobalContext();

  if (request->method != OrthancPluginHttpMethod_Delete)
  {
      return OrthancPluginSendMethodNotAllowed(context, output, "Delete");
  }

  std::string ids = request->groups[0];
  SaolaDatabase::Instance().DeleteFailedJobByIds(ids);

  Json::Value answer = Json::objectValue;
  std::string s = answer.toStyledString();
  OrthancPluginAnswerBuffer(context, output, s.c_str(), s.size(), "application/json");
}

void HandleFailedJobService(OrthancPluginRestOutput *output,
                            const char *url,
                            const OrthancPluginHttpRequest *request)
{
  if (request->method == OrthancPluginHttpMethod_Post)
  {
    SaveFailedJob(output, url, request);
  }
  else if (request->method == OrthancPluginHttpMethod_Put)
  {
    ResetFailedJob(output, url, request);
  }
  else if (request->method == OrthancPluginHttpMethod_Get)
  {
    GetFailedJobs(output, url, request);
  }
  else
  {
    return OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST, PUT, GET");
  }
}