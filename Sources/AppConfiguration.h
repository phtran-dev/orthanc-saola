#pragma once

#include <string>
#include <map> 

struct AppConfiguration
{
  std::string id_;

  bool enable_;

  std::string type_;

  unsigned int delay_ = 0;

  std::string url_;

  std::string authentication_;

  std::map<std::string, std::string> fieldMapping_;

  std::map<std::string, std::string> fieldValues_;

  AppConfiguration()
  {
  }

  void clone(AppConfiguration& that) const
  {
    that.id_ = this->id_;
    that.enable_ = this->enable_;
    that.type_ = this->type_;
    that.delay_ = this->delay_;
    that.url_ = this->url_;
    that.authentication_ = this->authentication_;
    for (const auto& m : this->fieldMapping_)
    {
      that.fieldMapping_[m.first] = m.second;
    }
    for (const auto& m : this->fieldValues_)
    {
      that.fieldValues_[m.first] = m.second;
    }
  }

};