#pragma once

#include "StableEventDTOGet.h"

#include <thread>
#include <boost/noncopyable.hpp>

class StableEventScheduler
{
private:
  enum State
  {
    State_Setup,
    State_Running,
    State_Done
  };

  std::thread *m_worker1;

  std::thread *m_worker2;

  State m_state;

  static void Worker(const State *state);

  StableEventScheduler() : m_state(State_Setup)
  {
  }

public:
  static StableEventScheduler &Instance();

  bool ExecuteEvent(StableEventDTOGet &event);

  ~StableEventScheduler();

  void Start();

  void Stop();
};