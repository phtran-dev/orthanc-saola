#pragma once

struct Pagination
{
  /* data */
  unsigned int offset_ = 0;
  unsigned int limit_ = 100;
  std::string  sort_by_ = "id";
};
