#pragma once

#include <boost/noncopyable.hpp>
#include <string>

namespace Saola
{
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
}
