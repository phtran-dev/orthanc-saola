#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

static boost::posix_time::ptime GetNow()
{
  return boost::posix_time::second_clock::universal_time();
}
