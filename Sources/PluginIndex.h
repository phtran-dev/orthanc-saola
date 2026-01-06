#pragma once

#include <string>
#include <list>

#include <Enumerations.h>

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <FileStorage/FileInfo.h>

class PluginIndex
{
private:
  PluginIndex()
  {}

public:
  static PluginIndex &Instance();

  bool LookupParent(std::string &target,
                    const std::string &publicId);

  bool LookupResourceType(Orthanc::ResourceType &type,
                          const std::string &publicId);

  void GetChildren(std::list<std::string> &result,
                   const std::string &publicId);

  bool LookupAttachment(Orthanc::FileInfo &attachment,
                        int64_t &revision,
                        const std::string &instancePublicId,
                        Orthanc::FileContentType contentType);

  bool GetMainDicomTags(Json::Value& tags, const std::string& publicId, Orthanc::ResourceType resourceIdLevel);

  bool ReadDicom(std::string& dicom, const std::string& instancePublicId);
};