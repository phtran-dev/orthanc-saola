#include "InMemoryJobCache.h"

InMemoryJobCache &InMemoryJobCache::Instance()
{
  static InMemoryJobCache instance;
  return instance;
}

void InMemoryJobCache::Insert(const std::string &jobId, const Json::Value &job)
{
  boost::mutex::scoped_lock lock(mutex_);
  jobs_[jobId] = job;
}

void InMemoryJobCache::Delete(const std::string &jobId)
{
  boost::mutex::scoped_lock lock(mutex_);
  jobs_.erase(jobId);
}

size_t InMemoryJobCache::GetSize()
{
  boost::mutex::scoped_lock lock(mutex_);
  return jobs_.size();
}

void InMemoryJobCache::GetJobs(Json::Value& results)
{
  for (const auto& job : jobs_)
  {
    results.append(job.second);
  }
}