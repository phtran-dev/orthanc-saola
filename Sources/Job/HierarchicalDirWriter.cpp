/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/

#include "HierarchicalDirWriter.h"

#include <Toolbox.h>
#include <SystemToolbox.h>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

namespace Saola
{
  std::string HierarchicalDirWriter::Index::KeepAlphanumeric(const std::string &source)
  {
    std::string result;

    bool lastSpace = false;

    result.reserve(source.size());
    for (size_t i = 0; i < source.size(); i++)
    {
      char c = source[i];
      if (c == '^')
        c = ' ';

      if (c <= 127 &&
          c >= 0)
      {
        if (isspace(c))
        {
          if (!lastSpace)
          {
            lastSpace = true;
            result.push_back(' ');
          }
        }
        else if (isalnum(c) ||
                 c == '.' ||
                 c == '_')
        {
          result.push_back(c);
          lastSpace = false;
        }
      }
    }

    return Orthanc::Toolbox::StripSpaces(result);
  }

  std::string HierarchicalDirWriter::Index::GetCurrentDirectoryPath() const
  {
    std::string result;

    Stack::const_iterator it = stack_.begin();
    ++it; // Skip the root node (to avoid absolute paths)

    while (it != stack_.end())
    {
      result += (*it)->name_ + "/";
      ++it;
    }

    return result;
  }

  std::string HierarchicalDirWriter::Index::EnsureUniqueFilename(const char *filename)
  {
    std::string standardized = KeepAlphanumeric(filename);

    Directory &d = *stack_.back();
    Directory::Content::iterator it = d.content_.find(standardized);

    if (it == d.content_.end())
    {
      d.content_[standardized] = 1;
      return standardized;
    }
    else
    {
      it->second++;
      return standardized + "-" + boost::lexical_cast<std::string>(it->second);
    }
  }

  HierarchicalDirWriter::Index::Index()
  {
    stack_.push_back(new Directory);
  }

  HierarchicalDirWriter::Index::~Index()
  {
    for (Stack::iterator it = stack_.begin(); it != stack_.end(); ++it)
    {
      delete *it;
    }
  }

  bool HierarchicalDirWriter::Index::IsRoot() const
  {
    return stack_.size() == 1;
  }

  std::string HierarchicalDirWriter::Index::OpenFile(const char *name)
  {
    return GetCurrentDirectoryPath() + EnsureUniqueFilename(name);
  }

  void HierarchicalDirWriter::Index::OpenDirectory(const char *name)
  {
    std::string d = EnsureUniqueFilename(name);

    // Push the new directory onto the stack
    stack_.push_back(new Directory);
    stack_.back()->name_ = d;
  }

  void HierarchicalDirWriter::Index::CloseDirectory()
  {
    if (IsRoot())
    {
      // Cannot close the root node
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadSequenceOfCalls);
    }

    delete stack_.back();
    stack_.pop_back();
  }

  HierarchicalDirWriter::HierarchicalDirWriter(const std::string& root) : root_(root)
  {
  }

  HierarchicalDirWriter::~HierarchicalDirWriter()
  {
  }

  void HierarchicalDirWriter::SetZip64(bool isZip64)
  {
  }

  bool HierarchicalDirWriter::IsZip64() const
  {
    return true;
  }

  void HierarchicalDirWriter::SetCompressionLevel(uint8_t level)
  {
  }

  uint8_t HierarchicalDirWriter::GetCompressionLevel() const
  {
    return 1;
  }

  void HierarchicalDirWriter::SetAppendToExisting(bool append)
  {
  }

  bool HierarchicalDirWriter::IsAppendToExisting() const
  {
    return true;
  }

  void HierarchicalDirWriter::OpenFile(const char *name)
  {
  }

  void HierarchicalDirWriter::OpenDirectory(const char *name)
  {
    indexer_.OpenDirectory(name);
    std::string p = indexer_.GetCurrentDirectoryPath();

    boost::filesystem::path path = root_;
    path /= p;
    boost::filesystem::create_directories(path);
  }

  void HierarchicalDirWriter::CloseDirectory()
  {
    indexer_.CloseDirectory();
  }

  std::string HierarchicalDirWriter::GetCurrentDirectoryPath() const
  {
    return indexer_.GetCurrentDirectoryPath();
  }

  void HierarchicalDirWriter::Write(const void *data, size_t length)
  {
  }

  void HierarchicalDirWriter::Write(const std::string &data, const char *name)
  {
    boost::filesystem::path path = this->root_;
    path /= indexer_.OpenFile(name);
    Orthanc::SystemToolbox::WriteFile(data.c_str(), data.size(), path.string(), false);
  }

  void HierarchicalDirWriter::Close()
  {
  }

  uint64_t HierarchicalDirWriter::GetDirectorySize() const
  {
    // @TODO
    return 1;
    // return Orthanc::SystemToolbox::GetFileSize(this->root_);
  }
}
