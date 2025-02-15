#pragma once

#include <map>
#include <string>

#include <json/value.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>

class InMemoryJobCache : public boost::noncopyable
{
private:
  boost::mutex mutex_;

  std::map<std::string, Json::Value> jobs_;

  InMemoryJobCache()
  {
  }

public:
  static InMemoryJobCache& Instance();  

  void Insert(const std::string& jobId, const Json::Value& job);

  void Delete(const std::string& jobId);

  size_t GetSize();

  void GetJobs(Json::Value& results);

};