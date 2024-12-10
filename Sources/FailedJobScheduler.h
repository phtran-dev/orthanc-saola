#pragma once

#include <thread>
#include <boost/noncopyable.hpp>

class FailedJobScheduler
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

    FailedJobScheduler() : m_state(State_Setup)
	  {
	  }

    void MonitorDatabase();

public:
    static FailedJobScheduler& Instance();

    ~FailedJobScheduler();

    void Start();
  
    void Stop();

};