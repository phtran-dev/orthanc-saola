#pragma once

#include <string>

void OnJobSuccess(const std::string &jobId);

void OnJobFailure(const std::string &jobId);