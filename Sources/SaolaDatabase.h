/**
 * Indexer plugin for Orthanc
 * Copyright (C) 2021-2024 Sebastien Jodogne, UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "StableEventDTOCreate.h"
#include "StableEventDTOUpdate.h"
#include "StableEventDTOGet.h"

#include "FailedJobDTOCreate.h"
#include "FailedJobDTOGet.h"

#include "FailedJobFilter.h"

#include "Pagination.h"

#include <list>

#include <OrthancFramework.h>  // To have ORTHANC_ENABLE_SQLITE defined
#include <SQLite/Connection.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>



class SaolaDatabase : public boost::noncopyable
{
public:
  enum FileStatus
  {
    FileStatus_New,
    FileStatus_Modified,
    FileStatus_AlreadyStored,
    FileStatus_NotDicom
  };

  class IFileVisitor : public boost::noncopyable
  {
  public:
    virtual ~IFileVisitor()
    {
    }

    virtual void VisitInstance(const std::string& path,
                               bool isDicom,
                               const std::string& instanceId) = 0;
  };  


private:
  boost::mutex                 mutex_;
  Orthanc::SQLite::Connection  db_;
  
  void Initialize();

  void AddFileInternal(const std::string& path,
                       const std::time_t time,
                       const uintmax_t size,
                       bool isDicom,
                       const std::string& instanceId);

  SaolaDatabase()
  {}

public:

  static SaolaDatabase& Instance();

  void Open(const std::string& path);

  void OpenInMemory();  // For unit tests

  FileStatus LookupFile(std::string& oldInstanceId,
                        const std::string& path,
                        const std::time_t time,
                        const uintmax_t size);


  // Warning: The visitor is invoked in mutual exclusion, so it
  // shouldn't do lengthy operations
  void Apply(IFileVisitor& visitor);

  // Returns "false" iff. this instance has not been previously
  // registerded using "AddDicomInstance()", which indicates the
  // import of an external DICOM file
  int64_t AddEvent(const StableEventDTOCreate& obj);

  bool DeleteEventByIds(const std::list<int> ids);

  bool DeleteEventByIds(const std::string& ids);

  bool UpdateEvent(const StableEventDTOUpdate& obj);

  void FindAll(const Pagination& page, std::list<StableEventDTOGet>& results);

  void FindByRetryLessThan(int retry, std::list<StableEventDTOGet>& results);

  void SaveFailedJob(const FailedJobDTOCreate& dto, FailedJobDTOGet& result);

  void FindAll(const Pagination& page, const FailedJobFilter& filter, std::list<FailedJobDTOGet>& results);

  bool ResetFailedJob(const std::list<std::string>& ids);

  bool DeleteFailedJobByIds(const std::string& ids);

};
