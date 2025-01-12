#include "RemoveFileScheduler.h"
#include "SaolaDatabase.h"
#include "TimeUtil.h"
#include "SaolaConfiguration.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>
#include <Enumerations.h>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/join.hpp>

#include <stack>
#include <sys/stat.h>

static const std::string EXPORTER_APP_TYPE = "Exporter";
static const std::string EXPORTER_DIR = "ExportDir";
static const std::string RETENTION_EXPIRED = "RetentionExpired";

static int GetRententionExpired(const std::map<std::string, int> &folders, const std::string &path)
{
  for (const auto &folder : folders)
  {
    if (path.find(folder.first) != std::string::npos)
    {
      return folder.second;
    }
  }
  LOG(INFO) << "[GetRententionExpired] Return default expired 48 hours of path: " << path;
  return 2 * 24; // Return default
}

RemoveFileScheduler &RemoveFileScheduler::Instance()
{
  static RemoveFileScheduler instance;
  return instance;
}

RemoveFileScheduler::~RemoveFileScheduler()
{
  if (this->m_state == State_Running)
  {
    OrthancPlugins::LogError("RemoveFileScheduler::Stop() should have been manually called");
    Stop();
  }
}

void RemoveFileScheduler::DeletePath(const std::string &path, int expirationDuration_)
{
  boost::posix_time::ptime lastModification =
      boost::posix_time::from_time_t(boost::filesystem::last_write_time(path));
  boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();

  // Be sure time
  if (now - lastModification < boost::posix_time::hours(expirationDuration_))
  {
    LOG(INFO) << "[RemoveFileScheduler::DeletePath] path=" << path << " is not expired yet, expirationDuration_=" << expirationDuration_ << "(hours), now=" << now << ", lastModification=" << lastModification;
    return;
  }

  if (boost::filesystem::is_directory(path) && !boost::filesystem::is_empty(path))
  {
    LOG(INFO) << "[RemoveFileScheduler::DeletePath] path=" << path << " is folder and not empty";
    return;
  }

  LOG(INFO) << "[RemoveFileScheduler::DeleteFolder] Deleting path : " << path << " , now : " << now << ", lastModification : " << lastModification;
  boost::system::error_code ec;
  boost::filesystem::remove_all(path, ec);
  if (ec)
  {
    LOG(ERROR) << "[RemoveFileScheduler::DeleteFolder] ERROR Cannot delete path: " << path;
    return;
  }

  LOG(INFO) << "[RemoveFileScheduler::DeleteFolder] Deleted path: " << path;
}

void RemoveFileScheduler::MonitorDirectories(const std::map<std::string, int> &folders)
{
  std::list<std::string> dirInfo;
  for (const auto &folder : folders)
  {
    dirInfo.push_back(folder.first + ":" + std::to_string(folder.second) + "(hours)");
  }

  while (this->m_state == State_Running)
  {
    LOG(INFO) << "[RemoveFileScheduler::MonitorDirectories] Start monitoring and removing folders: " << boost::algorithm::join(dirInfo, ", ");

    std::stack<boost::filesystem::path> s;
    for (const auto &folder : folders)
    {
      s.push(folder.first);
    }

    while (!s.empty())
    {
      if (this->m_state != State_Running)
      {
        return;
      }

      boost::filesystem::path d = s.top();
      s.pop();

      boost::filesystem::directory_iterator current;

      try
      {
        current = boost::filesystem::directory_iterator(d);
      }
      catch (boost::filesystem::filesystem_error &)
      {
        LOG(ERROR) << "[RemoveFileScheduler::MonitorDirectories] ERROR Cannot read directory: " << d.string();
        continue;
      }

      const boost::filesystem::directory_iterator end;

      while (current != end)
      {
        try
        {
          const boost::filesystem::file_status status = boost::filesystem::status(current->path());

          switch (status.type())
          {
          case boost::filesystem::regular_file:
          case boost::filesystem::reparse_file:
            DeletePath(current->path().string(), GetRententionExpired(folders, current->path().string()));
            break;
          case boost::filesystem::directory_file:
            s.push(current->path());
            break;

          default:
            break;
          }
        }
        catch (boost::filesystem::filesystem_error &e)
        {
          LOG(ERROR) << "[RemoveFileScheduler::MonitorDirectories] ERROR Catch general error: " << e.what();
        }

        ++current;
      }

      // Remove Directory after awhile
      if (folders.find(d.string()) == folders.end())
      {
        // Delete obsolete folder
        DeletePath(d.string(), GetRententionExpired(folders, d.string()));
      }
    }

    unsigned int intervalSeconds = 60;
    for (unsigned int i = 0; i < intervalSeconds * 10; i++)
    {
      if (this->m_state != State_Running)
      {
        return;
      }

      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
}

void RemoveFileScheduler::Start()
{
  if (this->m_state != State_Setup)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
  }

  std::map<std::string, int> directories;

  for (const auto &app : SaolaConfiguration::Instance().GetApps())
  {
    if (app->type_ == EXPORTER_APP_TYPE)
    {
      // Process Exporter app
      if (app->fieldValues_.isMember(EXPORTER_DIR))
      {
        int expiredHours = 24 * 2; // 2 days
        if (app->fieldValues_.isMember(RETENTION_EXPIRED))
        {
          expiredHours = app->fieldValues_[RETENTION_EXPIRED].asInt();
        }
        std::string dir = app->fieldValues_[EXPORTER_DIR].asString();
        if (!dir.empty() && dir.length() > 0 && dir[dir.length() - 1] != '/')
        {
          dir += '/';
        }
        directories.emplace(dir, expiredHours);
      }
    }
  }

  if (directories.empty())
  {
    this->m_state = State_Done;
    return;
  }

  this->m_state = State_Running;
  this->m_worker = new std::thread([this, directories]()
                                   {
    while (this->m_state == State_Running)
    {
      this->MonitorDirectories(directories);
    } });
}

void RemoveFileScheduler::Stop()
{
  if (this->m_state == State_Running)
  {
    this->m_state = State_Done;
    if (this->m_worker->joinable())
      this->m_worker->join();
    delete this->m_worker;
  }
}
