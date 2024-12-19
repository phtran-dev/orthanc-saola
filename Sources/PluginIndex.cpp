#include "PluginIndex.h"

bool PluginIndex::LookupParent(std::string &target,
                               const std::string &publicId)
{
  Json::Value result;
  bool ok = false;
  if (OrthancPlugins::RestApiGet(result, "/instances/" + publicId + "/series", false) && !result.empty())
  {
    ok = true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/series/" + publicId + "/study", false) && !result.empty())
  {
    ok = true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/studies/" + publicId + "/patient", false) && !result.empty())
  {
    ok = true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/patients/" + publicId, false) && !result.empty())
  {
    ok = true;
  }

  if (ok)
  {
    target = result["ID"].asString();
  }

  return ok;
}

bool PluginIndex::LookupResourceType(Orthanc::ResourceType &type,
                                     const std::string &publicId)
{
  Json::Value result;
  if (OrthancPlugins::RestApiGet(result, "/instances/" + publicId, false) && !result.empty())
  {
    type = Orthanc::ResourceType_Instance;
    return true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/series/" + publicId, false) && !result.empty())
  {
    type = Orthanc::ResourceType_Series;
    return true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/studies/" + publicId, false) && !result.empty())
  {
    type = Orthanc::ResourceType_Study;
    return true;
  }
  else if (OrthancPlugins::RestApiGet(result, "/patients/" + publicId, false) && !result.empty())
  {
    type = Orthanc::ResourceType_Patient;
    return true;
  }
  return false;
}

void PluginIndex::GetChildren(std::list<std::string> &result,
                              const std::string &publicId)
{
  Json::Value response = Json::objectValue;
  if (OrthancPlugins::RestApiGet(response, "/patients/" + publicId, false) && !response.empty())
  {
    for (const auto &study : response["Studies"])
    {
      result.push_back(study.asString());
    }
    return;
  }
  else if (OrthancPlugins::RestApiGet(response, "/studies/" + publicId, false) && !response.empty())
  {
    for (const auto &study : response["Series"])
    {
      result.push_back(study.asString());
    }
    return;
  }
  else if (OrthancPlugins::RestApiGet(response, "/series/" + publicId, false) && !response.empty())
  {
    for (const auto &study : response["Instances"])
    {
      result.push_back(study.asString());
    }
    return;
  }
}

bool PluginIndex::LookupAttachment(Orthanc::FileInfo &attachment,
                                   int64_t &revision,
                                   const std::string &instancePublicId,
                                   Orthanc::FileContentType contentType)
{
  Json::Value response = Json::objectValue;
  if (OrthancPlugins::RestApiGet(response, "/instances/" + instancePublicId + "/attachments/dicom/info", false) && !response.empty())
  {
      attachment = Orthanc::FileInfo(response["Uuid"].asString(),
                            Orthanc::FileContentType_Dicom,
                            response["UncompressedSize"].asInt(),
                            response["UncompressedMD5"].asString(),
                            Orthanc::CompressionType_None,
                            response["CompressedSize"].asInt(),
                            response["CompressedMD5"].asString());
    return true;
  }
  return false;
}

bool PluginIndex::ReadDicom(std::string& dicom, const std::string& instancePublicId)
{
  return OrthancPlugins::RestApiGetString(dicom, "/instances/" + instancePublicId + "/file", false);
}

bool PluginIndex::GetMainDicomTags(Json::Value& tags, const std::string& publicId, Orthanc::ResourceType level)
{
  std::string resource = "";
  switch (level)
  {
  case Orthanc::ResourceType_Study:
    resource = "/studies/";
    break;
  case Orthanc::ResourceType_Series:
    resource = "/series/";
    break;
  default:
    break;
  }

  if (resource.empty()) return false;

  Json::Value result;
  if (OrthancPlugins::RestApiGet(result, resource + publicId, false) && !result.empty())
  {
    tags.copy(result["MainDicomTags"]);
    return true;
  }
  return false;
}

PluginIndex &PluginIndex::Instance()
{
  static PluginIndex instance;
  return instance;
}
