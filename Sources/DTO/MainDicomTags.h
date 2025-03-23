#pragma once

#include <string>

struct MainDicomTags
{
  /* data */
  std::string remoteAET_;

  std::string remoteIP_;

  std::string accessionNumber_;

  std::string patientID_;

  std::string patientName_;

  std::string patientAge_;

  std::string patientBirthDate_; 

  std::string bodyPartExamined_;

  std::string studyDescription_;

  std::string patientSex_;

  std::string institutionName_;

  std::string studyDate_;

  std::string StudyTime_;

  std::string studyInstanceUID_;

  std::string manufacturerModelName_;

  std::string modality_;

  std::string countInstances_;

  std::string countSeries_;

  std::string operatorsName_;

  std::string referringPhysicianName_;

  std::string stationName_;

  MainDicomTags()
  {
  }

};
