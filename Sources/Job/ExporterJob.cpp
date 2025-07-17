#include "ExporterJob.h"
#include "../PluginIndex.h"
#include "HierarchicalDirWriter.h"

#include <Cache/SharedArchive.h>
#include <Compression/HierarchicalZipWriter.h>
#include <OrthancException.h>
#include <MultiThreading/Semaphore.h>
#include <MultiThreading/SharedMessageQueue.h>
#include <IDynamicObject.h>
#include <Logging.h>
#include <Toolbox.h>

#include <stdio.h>
#include <boost/range/algorithm/count.hpp>

#include <boost/filesystem.hpp>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

static const uint64_t MEGA_BYTES = 1024 * 1024;
static const uint64_t GIGA_BYTES = 1024 * 1024 * 1024;

static const char *const MEDIA_IMAGES_FOLDER = "IMAGES";
static const char *const KEY_DESCRIPTION = "Description";
static const char *const KEY_INSTANCES_COUNT = "InstancesCount";
static const char *const KEY_UNCOMPRESSED_SIZE_MB = "UncompressedSizeMB";
static const char *const KEY_UNCOMPRESSED_SIZE = "UncompressedSize";
static const char *const KEY_TRANSCODE = "Transcode";

static const char *const KEY_DIRECTORY_SIZE_MB = "DirectorySizeMB";
static const char *const KEY_DIRECTORY_SIZE = "DirectorySize";

static const char* const KEY_RESOURCES = "Resources";

namespace Saola
{
  class ExporterJob::InstanceLoader : public boost::noncopyable
  {
  protected:
    bool transcode_;
    Orthanc::DicomTransferSyntax transferSyntax_;

  public:
    explicit InstanceLoader(bool transcode, Orthanc::DicomTransferSyntax transferSyntax)
        : transcode_(transcode),
          transferSyntax_(transferSyntax)
    {
    }

    virtual ~InstanceLoader()
    {
    }

    virtual void PrepareDicom(const std::string &instanceId)
    {
    }

    bool TranscodeDicom(std::string &transcodedBuffer, const std::string &sourceBuffer, const std::string &instanceId)
    {
      if (transcode_)
      {
        // std::set<Orthanc::DicomTransferSyntax> syntaxes;
        // syntaxes.insert(transferSyntax_);

        // IDicomTranscoder::DicomImage source, transcoded;
        // source.SetExternalBuffer(sourceBuffer);

        // if (context_.Transcode(transcoded, source, syntaxes, true /* allow new SOP instance UID */))
        // {
        //   transcodedBuffer.assign(reinterpret_cast<const char*>(transcoded.GetBufferData()), transcoded.GetBufferSize());
        //   return true;
        // }
        // else
        // {
        //   LOG(INFO) << "Cannot transcode instance " << instanceId
        //             << " to transfer syntax: " << GetTransferSyntaxUid(transferSyntax_);
        // }
      }

      return false;
    }

    virtual void GetDicom(std::string &dicom, const std::string &instanceId) = 0;

    virtual void Clear()
    {
    }
  };

  class ExporterJob::SynchronousInstanceLoader : public ExporterJob::InstanceLoader
  {
  public:
    explicit SynchronousInstanceLoader(bool transcode, Orthanc::DicomTransferSyntax transferSyntax)
        : InstanceLoader(transcode, transferSyntax)
    {
    }

    virtual void GetDicom(std::string &dicom, const std::string &instanceId) ORTHANC_OVERRIDE
    {
      PluginIndex::Instance().ReadDicom(dicom, instanceId);
    }
  };

  class InstanceId : public Orthanc::IDynamicObject
  {
  private:
    std::string id_;

  public:
    explicit InstanceId(const std::string &id) : id_(id)
    {
    }

    virtual ~InstanceId() ORTHANC_OVERRIDE
    {
    }

    std::string GetId() const { return id_; };
  };

  class ExporterJob::ThreadedInstanceLoader : public ExporterJob::InstanceLoader
  {
    Orthanc::Semaphore availableInstancesSemaphore_;
    Orthanc::Semaphore bufferedInstancesSemaphore_;
    std::map<std::string, boost::shared_ptr<std::string>> availableInstances_;
    boost::mutex availableInstancesMutex_;
    Orthanc::SharedMessageQueue instancesToPreload_;
    std::vector<boost::thread *> threads_;

  public:
    ThreadedInstanceLoader(size_t threadCount, bool transcode, Orthanc::DicomTransferSyntax transferSyntax)
        : InstanceLoader(transcode, transferSyntax),
          availableInstancesSemaphore_(0),
          bufferedInstancesSemaphore_(3 * threadCount)
    {
      for (size_t i = 0; i < threadCount; i++)
      {
        threads_.push_back(new boost::thread(PreloaderWorkerThread, this));
      }
    }

    virtual ~ThreadedInstanceLoader()
    {
      Clear();
    }

    virtual void Clear() ORTHANC_OVERRIDE
    {
      for (size_t i = 0; i < threads_.size(); i++)
      {
        instancesToPreload_.Enqueue(NULL);
      }

      for (size_t i = 0; i < threads_.size(); i++)
      {
        if (threads_[i]->joinable())
        {
          threads_[i]->join();
        }
        delete threads_[i];
      }

      threads_.clear();
      availableInstances_.clear();
    }

    static void PreloaderWorkerThread(ThreadedInstanceLoader *that)
    {
      static uint16_t threadCounter = 0;
      Orthanc::Logging::SetCurrentThreadName(std::string("ARCH-LOAD-") + boost::lexical_cast<std::string>(threadCounter++));

      while (true)
      {
        std::unique_ptr<InstanceId> instanceId(dynamic_cast<InstanceId *>(that->instancesToPreload_.Dequeue(0)));
        if (instanceId.get() == NULL) // that's the signal to exit the thread
        {
          return;
        }

        // wait for the consumers (zip writer), no need to accumulate instances in memory if loaders are faster than writers
        that->bufferedInstancesSemaphore_.Acquire();
        try
        {
          boost::shared_ptr<std::string> dicomContent(new std::string());
          PluginIndex::Instance().ReadDicom(*dicomContent, instanceId->GetId());

          if (that->transcode_)
          {
            boost::shared_ptr<std::string> transcodedDicom(new std::string());
            if (that->TranscodeDicom(*transcodedDicom, *dicomContent, instanceId->GetId()))
            {
              dicomContent = transcodedDicom;
            }
          }

          {
            boost::mutex::scoped_lock lock(that->availableInstancesMutex_);
            that->availableInstances_[instanceId->GetId()] = dicomContent;
          }

          that->availableInstancesSemaphore_.Release();
        }
        catch (Orthanc::OrthancException &e)
        {
          boost::mutex::scoped_lock lock(that->availableInstancesMutex_);
          // store a NULL result to notify that we could not read the instance
          that->availableInstances_[instanceId->GetId()] = boost::shared_ptr<std::string>();
          that->availableInstancesSemaphore_.Release();
        }
      }
    }

    virtual void PrepareDicom(const std::string &instanceId) ORTHANC_OVERRIDE
    {
      instancesToPreload_.Enqueue(new InstanceId(instanceId));
    }

    virtual void GetDicom(std::string &dicom, const std::string &instanceId) ORTHANC_OVERRIDE
    {
      while (true)
      {
        // wait for an instance to be available but this might not be the one we are waiting for !
        availableInstancesSemaphore_.Acquire();
        bufferedInstancesSemaphore_.Release(); // unlock the "flow" of loaders

        boost::shared_ptr<std::string> dicomContent;
        {
          if (availableInstances_.find(instanceId) != availableInstances_.end())
          {
            // this is the instance we were waiting for
            dicomContent = availableInstances_[instanceId];
            availableInstances_.erase(instanceId);

            if (dicomContent.get() == NULL) // there has been an error while reading the file
            {
              throw Orthanc::OrthancException(Orthanc::ErrorCode_InexistentItem);
            }
            dicom.swap(*dicomContent);

            if (availableInstances_.size() > 0)
            {
              // we have just read the instance we were waiting for but there are still other instances available ->
              // make sure the next GetDicom call does not wait !
              availableInstancesSemaphore_.Release();
            }
            return;
          }
          // we have not found the expected instance, simply wait for the next loader thread to signal the semaphore when
          // a new instance is available
        }
      }
    }
  };

  // This enum defines specific resource types to be used when exporting the archive.
  // It defines if we should use the PatientInfo from the Patient or from the Study.
  enum ArchiveResourceType
  {
    ArchiveResourceType_Study = 0,
    ArchiveResourceType_StudyInfoFromSeries = 1,
    ArchiveResourceType_Series = 2,
    ArchiveResourceType_Instance = 3
  };

  Orthanc::ResourceType GetResourceIdType(ArchiveResourceType type)
  {
    switch (type)
    {
    case ArchiveResourceType_Study:
      return Orthanc::ResourceType_Study;
    case ArchiveResourceType_StudyInfoFromSeries: // get the Patient tags from the Study id
      return Orthanc::ResourceType_Study;
    case ArchiveResourceType_Series:
      return Orthanc::ResourceType_Series;
    case ArchiveResourceType_Instance:
      return Orthanc::ResourceType_Instance;
    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
  }

  Orthanc::ResourceType GetResourceLevel(ArchiveResourceType type)
  {
    switch (type)
    {
    case ArchiveResourceType_Study:
      return Orthanc::ResourceType_Study;
    case ArchiveResourceType_StudyInfoFromSeries: // this is actually the same level as the Patient
      return Orthanc::ResourceType_Study;
    case ArchiveResourceType_Series:
      return Orthanc::ResourceType_Series;
    case ArchiveResourceType_Instance:
      return Orthanc::ResourceType_Instance;
    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
  }

  ArchiveResourceType GetArchiveResourceType(Orthanc::ResourceType type)
  {
    switch (type)
    {
    case Orthanc::ResourceType_Study:
      return ArchiveResourceType_Study;
    case Orthanc::ResourceType_Series:
      return ArchiveResourceType_StudyInfoFromSeries;
    case Orthanc::ResourceType_Instance:
      return ArchiveResourceType_Instance;
    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
  }

  ArchiveResourceType GetChildResourceType(ArchiveResourceType type)
  {
    switch (type)
    {
    case ArchiveResourceType_Study:
    case ArchiveResourceType_StudyInfoFromSeries:
      return ArchiveResourceType_Series;

    case ArchiveResourceType_Series:
      return ArchiveResourceType_Instance;

    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }
  }

  class ExporterJob::ResourceIdentifiers : public boost::noncopyable
  {
  public:
    Orthanc::ResourceType level_;

    std::string study_;
    std::string series_;
    std::string instance_;

    static void GoToParent(PluginIndex &index,
                           std::string &current)
    {
      std::string tmp;

      if (index.LookupParent(tmp, current))
      {
        current = tmp;
      }
      else
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource);
      }
    }

  public:
    ResourceIdentifiers(PluginIndex &index,
                        const std::string &publicId)
    {
      if (!index.LookupResourceType(level_, publicId))
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_UnknownResource);
      }

      std::string current = publicId;

      switch (level_) // Do not add "break" below!
      {
      case Orthanc::ResourceType_Instance:
        instance_ = current;
        GoToParent(index, current);

      case Orthanc::ResourceType_Series:
        series_ = current;
        GoToParent(index, current);

      case Orthanc::ResourceType_Study:
        study_ = current;
        break;

      default:
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
    }

    Orthanc::ResourceType GetLevel() const
    {
      return level_;
    }

    const std::string &GetIdentifier(Orthanc::ResourceType level) const
    {
      // Some sanity check to ensure enumerations are not altered
      assert(Orthanc::ResourceType_Patient < Orthanc::ResourceType_Study);
      assert(Orthanc::ResourceType_Study < Orthanc::ResourceType_Series);
      assert(Orthanc::ResourceType_Series < Orthanc::ResourceType_Instance);

      if (level > level_)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }

      switch (level)
      {
      case Orthanc::ResourceType_Study:
        return study_;

      case Orthanc::ResourceType_Series:
        return series_;

      case Orthanc::ResourceType_Instance:
        return instance_;

      default:
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
    }
  };

  class ExporterJob::IArchiveVisitor : public boost::noncopyable
  {
  public:
    virtual ~IArchiveVisitor()
    {
    }

    virtual void Open(ArchiveResourceType level,
                      const std::string &publicId) = 0;

    virtual void Close() = 0;

    virtual void AddInstance(const std::string &instanceId,
                             uint64_t uncompressedSize) = 0;
  };

  class ExporterJob::ArchiveIndex : public boost::noncopyable
  {
  private:
    struct Instance
    {
      std::string id_;
      uint64_t uncompressedSize_;

      Instance(const std::string &id,
               uint64_t uncompressedSize) : id_(id),
                                            uncompressedSize_(uncompressedSize)
      {
      }
    };

    // A "NULL" value for ArchiveIndex indicates a non-expanded node
    typedef std::map<std::string, ArchiveIndex *> Resources;

    ArchiveResourceType level_;
    Resources resources_;           // Only at patient/study/series level
    std::list<Instance> instances_; // Only at instance level

    void AddResourceToExpand(PluginIndex &index,
                             const std::string &id)
    {
      if (level_ == ArchiveResourceType_Instance)
      {
        Orthanc::FileInfo tmp;
        int64_t revision; // ignored
        if (index.LookupAttachment(tmp, revision, id, Orthanc::FileContentType_Dicom))
        {
          instances_.push_back(Instance(id, tmp.GetUncompressedSize()));
        }
      }
      else
      {
        resources_[id] = NULL;
      }
    }

  public:
    explicit ArchiveIndex(ArchiveResourceType level) : level_(level)
    {
    }

    ~ArchiveIndex()
    {
      for (Resources::iterator it = resources_.begin();
           it != resources_.end(); ++it)
      {
        delete it->second;
      }
    }

    void Add(PluginIndex &index,
             const ResourceIdentifiers &resource)
    {
      const std::string &id = resource.GetIdentifier(GetResourceIdType(level_));
      Resources::iterator previous = resources_.find(id);

      if (level_ == ArchiveResourceType_Instance)
      {
        AddResourceToExpand(index, id);
      }
      else if (resource.GetLevel() == GetResourceLevel(level_))
      {
        // Mark this resource for further expansion
        if (previous != resources_.end())
        {
          delete previous->second;
        }

        resources_[id] = NULL;
      }
      else if (previous == resources_.end())
      {
        // This is the first time we meet this resource
        std::unique_ptr<ArchiveIndex> child(new ArchiveIndex(GetChildResourceType(level_)));
        child->Add(index, resource);
        resources_[id] = child.release();
      }
      else if (previous->second != NULL)
      {
        previous->second->Add(index, resource);
      }
      else
      {
        // Nothing to do: This item is marked for further expansion
      }
    }

    void Expand(PluginIndex &index)
    {
      if (level_ == ArchiveResourceType_Instance)
      {
        // Expanding an instance node makes no sense
        return;
      }

      for (Resources::iterator it = resources_.begin();
           it != resources_.end(); ++it)
      {
        if (it->second == NULL)
        {
          // This is resource is marked for expansion
          std::list<std::string> children;
          index.GetChildren(children, it->first);

          std::unique_ptr<ArchiveIndex> child(new ArchiveIndex(GetChildResourceType(level_)));

          for (std::list<std::string>::const_iterator
                   it2 = children.begin();
               it2 != children.end(); ++it2)
          {
            child->AddResourceToExpand(index, *it2);
          }

          it->second = child.release();
        }

        assert(it->second != NULL);
        it->second->Expand(index);
      }
    }

    void Apply(IArchiveVisitor &visitor) const
    {
      if (level_ == ArchiveResourceType_Instance)
      {
        for (std::list<Instance>::const_iterator
                 it = instances_.begin();
             it != instances_.end(); ++it)
        {
          visitor.AddInstance(it->id_, it->uncompressedSize_);
        }
      }
      else
      {
        for (Resources::const_iterator it = resources_.begin();
             it != resources_.end(); ++it)
        {
          assert(it->second != NULL); // There must have been a call to "Expand()"
          visitor.Open(level_, it->first);
          it->second->Apply(visitor);
          visitor.Close();
        }
      }
    }
  };

  // ------------------------------------------------------------------------
  class ExporterJob::DirectoryCommands : public boost::noncopyable
  {
  private:
    enum Type
    {
      Type_OpenDirectory,
      Type_CloseDirectory,
      Type_WriteInstance
    };

    class Command : public boost::noncopyable
    {
    private:
      Type type_;
      std::string filename_;
      std::string instanceId_;

    public:
      explicit Command(Type type) : type_(type)
      {
        assert(type_ == Type_CloseDirectory);
      }

      Command(Type type,
              const std::string &filename) : type_(type),
                                             filename_(filename)
      {
        assert(type_ == Type_OpenDirectory);
      }

      Command(Type type,
              const std::string &filename,
              const std::string &instanceId) : type_(type),
                                               filename_(filename),
                                               instanceId_(instanceId)
      {
        assert(type_ == Type_WriteInstance);
      }

      void Apply(Saola::HierarchicalDirWriter &writer,
                 InstanceLoader &instanceLoader,
                 bool transcode,
                 Orthanc::DicomTransferSyntax transferSyntax) const
      {
        switch (type_)
        {
        case Type_OpenDirectory:
          writer.OpenDirectory(filename_.c_str());
          break;

        case Type_CloseDirectory:
          writer.CloseDirectory();
          break;

        case Type_WriteInstance:
        {
          std::string content;

          try
          {
            instanceLoader.GetDicom(content, instanceId_);
          }
          catch (Orthanc::OrthancException &e)
          {
            LOG(WARNING) << "An instance was removed after the job was issued: " << instanceId_;
            return;
          }
          writer.Write(content, filename_.c_str());
          break;
        }

        default:
          throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
        }
      }
    };

    std::deque<Command *> commands_;
    uint64_t uncompressedSize_;
    unsigned int instancesCount_;
    InstanceLoader &instanceLoader_;

    void ApplyInternal(Saola::HierarchicalDirWriter &writer,
                       InstanceLoader &instanceLoader,
                       size_t index,
                       bool transcode,
                       Orthanc::DicomTransferSyntax transferSyntax) const
    {
      if (index >= commands_.size())
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
      }

      commands_[index]->Apply(writer, instanceLoader, transcode, transferSyntax);
    }

  public:
    explicit DirectoryCommands(InstanceLoader &instanceLoader) : uncompressedSize_(0),
                                                                 instancesCount_(0),
                                                                 instanceLoader_(instanceLoader)
    {
    }

    ~DirectoryCommands()
    {
      for (std::deque<Command *>::iterator it = commands_.begin();
           it != commands_.end(); ++it)
      {
        assert(*it != NULL);
        delete *it;
      }
    }

    size_t GetSize() const
    {
      return commands_.size();
    }

    unsigned int GetInstancesCount() const
    {
      return instancesCount_;
    }

    uint64_t GetUncompressedSize() const
    {
      return uncompressedSize_;
    }

    void Apply(Saola::HierarchicalDirWriter &writer,
               InstanceLoader &instanceLoader,
               size_t index,
               bool transcode,
               Orthanc::DicomTransferSyntax transferSyntax) const
    {
      ApplyInternal(writer, instanceLoader, index, transcode, transferSyntax);
    }

    void AddOpenDirectory(const std::string &filename)
    {
      commands_.push_back(new Command(Type_OpenDirectory, filename));
    }

    void AddCloseDirectory()
    {
      commands_.push_back(new Command(Type_CloseDirectory));
    }

    void AddWriteInstance(const std::string &filename,
                          const std::string &instanceId,
                          uint64_t uncompressedSize)
    {
      instanceLoader_.PrepareDicom(instanceId);
      commands_.push_back(new Command(Type_WriteInstance, filename, instanceId));
      instancesCount_++;
      uncompressedSize_ += uncompressedSize;
    }
  };

  // ---------------------------------------------------------------------------------------

  class ExporterJob::ArchiveIndexVisitor : public IArchiveVisitor
  {
  private:
    DirectoryCommands &commands_;
    char instanceFormat_[24];
    unsigned int counter_;

  public:
    ArchiveIndexVisitor(DirectoryCommands &commands) : commands_(commands),
                                                       counter_(0)
    {
      if (commands.GetSize() != 0)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
      }

      snprintf(instanceFormat_, sizeof(instanceFormat_) - 1, "%%08d.dcm");
    }

    virtual void Open(ArchiveResourceType level,
                      const std::string &publicId) ORTHANC_OVERRIDE
    {
      std::string path;
      Orthanc::ResourceType resourceIdLevel = GetResourceIdType(level);
      // Orthanc::ResourceType interestLevel = (level == ArchiveResourceType_StudyInfoFromSeries ? Orthanc::ResourceType_Study : resourceIdLevel);

      Json::Value tags;
      if (PluginIndex::Instance().GetMainDicomTags(tags, publicId, resourceIdLevel))
      {
        switch (level)
        {
        case ArchiveResourceType_Study:
        case ArchiveResourceType_StudyInfoFromSeries:
          path = tags["StudyInstanceUID"].asString();
          break;

        case ArchiveResourceType_Series:
          path = tags["SeriesInstanceUID"].asString();
          break;

          counter_ = 0;
          break;

        default:
          throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
        }
      }

      path = Orthanc::Toolbox::StripSpaces(Orthanc::Toolbox::ConvertToAscii(path));

      // if (path.empty() || (static_cast<size_t>(boost::count(path, '^')) == path.size())) // this happens with non ASCII patient names: only the '^' remains and this is not a valid zip folder name
      // {
      //   path = std::string("Unknown ") + EnumerationToString(GetResourceLevel(level));
      // }

      commands_.AddOpenDirectory(path.c_str());
    }

    virtual void Close() ORTHANC_OVERRIDE
    {
      commands_.AddCloseDirectory();
    }

    virtual void AddInstance(const std::string &instanceId,
                             uint64_t uncompressedSize) ORTHANC_OVERRIDE
    {
      // char filename[24];
      // snprintf(filename, sizeof(filename) - 1, instanceFormat_, counter_);
      counter_++;
      commands_.AddWriteInstance(instanceId + ".dcm", instanceId, uncompressedSize);
    }
  };

  // ---------------------------------------------------------------------------------------

  class ExporterJob::DirectoryWriterIterator : public boost::noncopyable
  {
  private:
    InstanceLoader &instanceLoader_;
    DirectoryCommands commands_;
    std::unique_ptr<Saola::HierarchicalDirWriter> dir_;

  public:
    DirectoryWriterIterator(InstanceLoader &instanceLoader,
                            ArchiveIndex &archive,
                            const std::string rootDir,
                            bool enableExtendedSopClass) : instanceLoader_(instanceLoader),
                                                           commands_(instanceLoader)
    {
      ArchiveIndexVisitor visitor(commands_);
      archive.Expand(PluginIndex::Instance());
      archive.Apply(visitor);
      dir_.reset(new Saola::HierarchicalDirWriter(rootDir)); // TODO hard code
    }

    void Close()
    {
      if (dir_.get() == NULL)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        dir_->Close();
      }
    }

    uint64_t GetDirectorySize() const
    {
      if (dir_.get() == NULL)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        return dir_->GetDirectorySize();
      }
    }

    size_t GetStepsCount() const
    {
      return commands_.GetSize() + 1;
    }

    void RunStep(size_t index,
                 bool transcode,
                 Orthanc::DicomTransferSyntax transferSyntax)
    {
      if (index > commands_.GetSize())
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
      }
      else if (dir_.get() == NULL)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
      }
      else if (index == commands_.GetSize())
      {
        // Last step: Do nothing. @TODO
      }
      else
      {
        commands_.Apply(*dir_, instanceLoader_, index, transcode, transferSyntax);
      }
    }

    unsigned int GetInstancesCount() const
    {
      return commands_.GetInstancesCount();
    }

    uint64_t GetUncompressedSize() const
    {
      return commands_.GetUncompressedSize();
    }
  };

  // ------------------------------------------------------------------------

  ExporterJob::ExporterJob(bool enableExtendedSopClass,
                           const std::string &rootDir,
                           Orthanc::ResourceType jobLevel) : OrthancPlugins::OrthancJob("Exporter"),
                                                             archive_(new ArchiveIndex(GetArchiveResourceType(jobLevel))),
                                                             enableExtendedSopClass_(enableExtendedSopClass),
                                                             rootDir_(rootDir)
  {
  }

  ExporterJob::~ExporterJob()
  {
  }

  void ExporterJob::SetDescription(const std::string &description)
  {
    if (writer_.get() != NULL) // Already started
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      description_ = description;
    }
  }

  void ExporterJob::AddResource(const std::string &publicId)
  {
    if (writer_.get() != NULL) // Already started
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    resourceIdentifiers_.reset(new ResourceIdentifiers(PluginIndex::Instance(), publicId));
    archive_->Add(PluginIndex::Instance(), *resourceIdentifiers_);
  }

  void ExporterJob::SetTranscode(Orthanc::DicomTransferSyntax transferSyntax)
  {
    if (writer_.get() != NULL) // Already started
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      this->transcode_ = true;
      this->transferSyntax_ = transferSyntax;
    }
  }

  void ExporterJob::SetLoaderThreads(unsigned int loaderThreads)
  {
    if (writer_.get() != NULL) // Already started
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      loaderThreads_ = loaderThreads;
    }
  }

  void ExporterJob::Reset()
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls,
                                    "Cannot resubmit the creation of an archive");
  }

  void ExporterJob::Start()
  {
    if (loaderThreads_ == 0)
    {
      // default behaviour before loaderThreads was introducted in 1.10.0
      instanceLoader_.reset(new SynchronousInstanceLoader(transcode_, transferSyntax_));
    }
    else
    {
      instanceLoader_.reset(new ThreadedInstanceLoader(loaderThreads_, transcode_, transferSyntax_));
    }

    if (writer_.get() != NULL)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      writer_.reset(new DirectoryWriterIterator(*instanceLoader_, *archive_, rootDir_, enableExtendedSopClass_));

      instancesCount_ = writer_->GetInstancesCount();
      uncompressedSize_ = writer_->GetUncompressedSize();
    }
  }

  namespace
  {
    class DynamicTemporaryFile : public Orthanc::IDynamicObject
    {
    private:
      std::unique_ptr<Orthanc::TemporaryFile> file_;

    public:
      explicit DynamicTemporaryFile(Orthanc::TemporaryFile *f) : file_(f)
      {
        if (f == NULL)
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_NullPointer);
        }
      }

      const Orthanc::TemporaryFile &GetFile() const
      {
        assert(file_.get() != NULL);
        return *file_;
      }
    };
  }

  void ExporterJob::FinalizeTarget()
  {
    directorySize_ = writer_->GetDirectorySize();
    if (writer_.get() != NULL)
    {
      writer_->Close(); // Flush all the results
      writer_.reset();
    }

    if (instanceLoader_.get() != NULL)
    {
      instanceLoader_->Clear();
    }

    {
      Json::Value value = Json::objectValue;
      value[KEY_DESCRIPTION] = description_;
      value[KEY_INSTANCES_COUNT] = instancesCount_;
      value[KEY_UNCOMPRESSED_SIZE_MB] =
          static_cast<unsigned int>(uncompressedSize_ / MEGA_BYTES);
      value[KEY_UNCOMPRESSED_SIZE] = boost::lexical_cast<std::string>(uncompressedSize_);
      value[KEY_DIRECTORY_SIZE] = directorySize_;
      value[KEY_DIRECTORY_SIZE_MB] =
          static_cast<unsigned int>(directorySize_ / MEGA_BYTES);
      value[KEY_RESOURCES] = Json::arrayValue;
      {
        Json::Value resource = Json::objectValue;
        resource["Study"] = resourceIdentifiers_->study_;
        resource["Series"] = resourceIdentifiers_->series_;
        resource["Instance"] = resourceIdentifiers_->instance_;
        resource["Level"] = Orthanc::EnumerationToString(resourceIdentifiers_->level_);
        value[KEY_RESOURCES].append(resource);
      }
      UpdateContent(value);
    }
  }

  OrthancPluginJobStepStatus ExporterJob::Step()
  {
    if (!isStarted_)
    {
      isStarted_ = true;
      Start();
      return OrthancPluginJobStepStatus_Continue;
    }

    assert(writer_.get() != NULL);

    {
      UpdateProgress(static_cast<float>(currentStep_) /
                     static_cast<float>(writer_->GetStepsCount() - 1));
    }

    if (writer_->GetStepsCount() == 0)
    {
      FinalizeTarget();
      return OrthancPluginJobStepStatus_Success;
    }
    else
    {
      try
      {
        writer_->RunStep(currentStep_, transcode_, transferSyntax_);
      }
      catch (Orthanc::OrthancException &e)
      {
        LOG(ERROR) << "[ExporterJob::Step] ERROR while creating an archive: " << e.What();
        throw;
      }

      currentStep_++;

      if (currentStep_ == writer_->GetStepsCount())
      {
        FinalizeTarget();
        return OrthancPluginJobStepStatus_Success;
      }
      else
      {
        return OrthancPluginJobStepStatus_Continue;
      }
    }
  }

  void ExporterJob::Stop(OrthancPluginJobStopReason reason)
  {
  }
}
