#pragma once

#include <thread>
#include <boost/thread.hpp>

#include <boost/noncopyable.hpp>
#include <map>

class PollingDBScheduler
{
private:
  enum State
  {
    State_Setup,
    State_Running,
    State_Done
  };

  boost::thread *m_worker;

  State m_state;

  static void Worker(const State *state);

  PollingDBScheduler() : m_state(State_Setup)
  {
  }

  void DeletePath(const std::string &path, int retentionExpired);

  void MonitorDatabase();

public:
  static PollingDBScheduler &Instance();

  ~PollingDBScheduler();

  void Start();

  void Stop();
};
