#pragma once

#include <string>

namespace Saola
{
  void OnJobSubmitted(const std::string &jobId);

  void OnJobSuccess(const std::string &jobId);

  void OnJobFailure(const std::string &jobId);
} // End of Saola
