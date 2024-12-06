#pragma once


struct FailedJobFilter
{
  int min_retry_;

  int max_retry_;
  
  FailedJobFilter(int min, int max) :
    min_retry_(min), max_retry_(max)
  {}

};