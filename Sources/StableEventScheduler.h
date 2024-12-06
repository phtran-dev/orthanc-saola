#pragma once

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

    std::thread* m_worker;

    State                       m_state;

    static void Worker(const State* state);

    StableEventScheduler() : m_state(State_Setup)
	  {
	  }

    void MonitorDatabase();

public:
    static StableEventScheduler& Instance();

    ~StableEventScheduler();

    void Start();
  
    void Stop();

};