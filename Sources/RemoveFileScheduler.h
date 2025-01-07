#pragma once

#include <thread>
#include <boost/noncopyable.hpp>
#include <map>

class RemoveFileScheduler
{
private:
  enum State
  {
    State_Setup,
    State_Running,
    State_Done
  };

  std::thread *m_worker;

  State m_state;

  static void Worker(const State *state);

  RemoveFileScheduler() : m_state(State_Setup)
  {
  }

  void DeletePath(const std::string &path, int retentionExpired);

  void MonitorDirectories(const std::map<std::string, int> &folders);

public:
  static RemoveFileScheduler &Instance();

  ~RemoveFileScheduler();

  void Start();

  void Stop();
};