#include "PluginIndex.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>

#include <Logging.h>

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
    if (response.isMember("Instances") && response["Instances"].isArray() && response["Instances"].size() > 0)
    {
      std::string firstInstanceId = response["Instances"][0].asString();
      LOG(INFO) << "PHONG firstInstanceId: " << firstInstanceId << std::endl;

      // Query the attachment info to get the actual file path
      Json::Value attachmentInfo;
      if (OrthancPlugins::RestApiGet(attachmentInfo, "/instances/" + firstInstanceId + "/attachments/dicom/info", true)
          && attachmentInfo.isMember("Path"))
      {
        std::string instancePath = attachmentInfo["Path"].asString();
        LOG(INFO) << "PHONG firstInstanceId: " << firstInstanceId << ", instancePath=" << instancePath << std::endl;
        
        // Get the parent directory (series folder) by going up one level
        boost::filesystem::path seriesFolder = boost::filesystem::path(instancePath).parent_path();
        
        // Count files in the series folder
        size_t fileCount = 0;
        std::list<std::string> filePaths;

        if (boost::filesystem::exists(seriesFolder) && boost::filesystem::is_directory(seriesFolder))
        {
          for (const auto &entry : boost::filesystem::directory_iterator(seriesFolder))
          {
            if (boost::filesystem::is_regular_file(entry.path()))
            {
              fileCount++;
              filePaths.push_back(entry.path().string());
            }
          }
        }
 
        // Check if the number of instances matches the number of files in the series folder
        size_t instanceCount = response["Instances"].size();
        LOG(INFO) << "PHONG instanceCount from response: " << instanceCount << ", fileCount in folder: " << fileCount;

        if (instanceCount == fileCount)
        {
          // If equal, use file scheme URI (file://path)
          LOG(INFO) << "PHONG Using file scheme - counts match";
          for (const auto &filePath : filePaths)
          {
            result.push_back("file://" + filePath);
          }
        }
        else
        {
          // If not equal, use orthanc scheme URI (orthanc://instanceId)
          LOG(INFO) << "PHONG Using orthanc scheme - counts don't match";
          for (const auto &instance : response["Instances"])
          {
            result.push_back("orthanc://" + instance.asString());
          }
        }
      }
      else
      {
        // Fallback: if we can't get the file path, use orthanc scheme
        LOG(WARNING) << "PHONG Cannot get file path, using orthanc scheme as fallback";
        for (const auto &instance : response["Instances"])
        {
          result.push_back("orthanc://" + instance.asString());
        }
      }
    }

    return;
  }
}

bool PluginIndex::LookupAttachment(Orthanc::FileInfo &attachment,
                                   int64_t &revision,
                                   const std::string &instancePublicId,
                                   Orthanc::FileContentType contentType)
{
  // Parse URI scheme to determine how to get attachment info
  if (instancePublicId.substr(0, 7) == "file://")
  {
    // File scheme: extract path and get file info directly
    std::string filePath = instancePublicId.substr(7); // Remove "file://" prefix
    
    if (boost::filesystem::exists(filePath) && boost::filesystem::is_regular_file(filePath))
    {
      try
      {
        uint64_t fileSize = boost::filesystem::file_size(filePath);
        
        // Create a FileInfo with basic information
        // Note: We don't have MD5 hashes for file scheme, so we use empty strings
        attachment = Orthanc::FileInfo(filePath,  // Use file path as UUID
                                      Orthanc::FileContentType_Dicom,
                                      fileSize,  // Uncompressed size
                                      "",        // UncompressedMD5 (not available)
                                      Orthanc::CompressionType_None,
                                      fileSize,  // Compressed size (same as uncompressed)
                                      "");       // CompressedMD5 (not available)
        revision = 0; // No revision for file scheme
        return true;
      }
      catch (const boost::filesystem::filesystem_error &e)
      {
        LOG(ERROR) << "Cannot get file info for: " << filePath << " - " << e.what();
        return false;
      }
    }
    else
    {
      LOG(ERROR) << "File does not exist: " << filePath;
      return false;
    }
  }
  else if (instancePublicId.substr(0, 10) == "orthanc://")
  {
    // Orthanc scheme: extract instance ID and use REST API
    std::string orthancInstanceId = instancePublicId.substr(10); // Remove "orthanc://" prefix
    Json::Value response = Json::objectValue;
    if (OrthancPlugins::RestApiGet(response, "/instances/" + orthancInstanceId + "/attachments/dicom/info", false) && !response.empty())
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
  else
  {
    // Fallback: assume it's an Orthanc instance ID without scheme (backward compatibility)
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
}

bool PluginIndex::ReadDicom(std::string& dicom, const std::string& instancePublicId)
{
  // Parse URI scheme to determine how to read the DICOM file
  if (instancePublicId.substr(0, 7) == "file://")
  {
    // File scheme: extract path and read directly from file
    std::string filePath = instancePublicId.substr(7); // Remove "file://" prefix
    std::ifstream file(filePath, std::ios::binary);
    if (file.is_open())
    {
      std::stringstream buffer;
      buffer << file.rdbuf();
      dicom = buffer.str();
      file.close();
      return true;
    }
    else
    {
      LOG(ERROR) << "Cannot read file: " << filePath;
      return false;
    }
  }
  else if (instancePublicId.substr(0, 10) == "orthanc://")
  {
    // Orthanc scheme: extract instance ID and use REST API
    std::string orthancInstanceId = instancePublicId.substr(10); // Remove "orthanc://" prefix
    return OrthancPlugins::RestApiGetString(dicom, "/instances/" + orthancInstanceId + "/file", false);
  }
  else
  {
    // Fallback: assume it's an Orthanc instance ID without scheme (backward compatibility)
    return OrthancPlugins::RestApiGetString(dicom, "/instances/" + instancePublicId + "/file", false);
  }
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
