#pragma once

#include <string>

#include <Enumerations.h>
#include <list>
#include <boost/shared_ptr.hpp>

#include <Compatibility.h>
#include <Compression/ZipWriter.h>
#include <TemporaryFile.h>

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace Saola
{
  class ExporterJob : public OrthancPlugins::OrthancJob
  {
  private:
    class ArchiveIndex;
    class ArchiveIndexVisitor;
    class IArchiveVisitor;
    class ResourceIdentifiers;
    class DirectoryCommands;
    class InstanceLoader;
    class SynchronousInstanceLoader;
    class ThreadedInstanceLoader;
    class DirectoryWriterIterator;

    std::unique_ptr<InstanceLoader> instanceLoader_;
    std::unique_ptr<ResourceIdentifiers> resourceIdentifiers_;
    boost::shared_ptr<ArchiveIndex> archive_;
    bool enableExtendedSopClass_ = false;
    std::string description_;
    std::string content_;

    boost::shared_ptr<DirectoryWriterIterator> writer_;
    size_t currentStep_ = 0;
    unsigned int instancesCount_ = 0;
    uint64_t uncompressedSize_ = 0;
    uint64_t directorySize_ = 0;
    std::string rootDir_;

    bool isStarted_ = false;
    bool transcode_;
    Orthanc::DicomTransferSyntax transferSyntax_ = Orthanc::DicomTransferSyntax_LittleEndianImplicit;

    unsigned int loaderThreads_ = 0;

    void FinalizeTarget();

  public:
    ExporterJob(bool enableExtendedSopClass,
                const std::string& rootDir,
                Orthanc::ResourceType jobLevel);



    void SetDescription(const std::string &description);

    const std::string &GetDescription() const
    {
      return description_;
    }

    void AddResource(const std::string &publicId);

    void SetTranscode(Orthanc::DicomTransferSyntax transferSyntax);

    void SetLoaderThreads(unsigned int loaderThreads);

    const std::string &GetContent() const;

    void Start();

    virtual void Reset() override;

    virtual OrthancPluginJobStepStatus Step() override;

    virtual void Stop(OrthancPluginJobStopReason reason);

    virtual ~ExporterJob();
  };
} // End of namespace Saola