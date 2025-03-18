#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

namespace Saola
{
  static boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::second_clock::universal_time();
  }

  static boost::posix_time::ptime GetNextXSecondsFromNow(int seconds)
  {
    return boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(seconds);
  }

  static std::string GetNextXSecondsFromNowInString(int seconds)
  {
    return boost::posix_time::to_iso_string(boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(seconds));
  }

  static bool IsOverDue(const std::string &time, int seconds)
  {
    return boost::posix_time::second_clock::universal_time() - boost::posix_time::from_iso_string(time) > boost::posix_time::seconds(seconds);
  }

  static auto Elapsed(const std::string &time)
  {
    return boost::posix_time::second_clock::universal_time() - boost::posix_time::from_iso_string(time);
  }
} // End of Saola
