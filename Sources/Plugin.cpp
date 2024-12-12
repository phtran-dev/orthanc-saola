/**
 * Indexer plugin for Orthanc
 * Copyright (C) 2021-2024 Sebastien Jodogne, UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/

#include "SaolaDatabase.h"
#include "StableEventScheduler.h"
#include "StableEventDTOCreate.h"
#include "MainDicomTags.h"
#include "SaolaConfiguration.h"
#include "Constants.h"
#include "RestApi.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Logging.h>
#include <SystemToolbox.h>

#include <boost/filesystem.hpp>

#define ORTHANC_PLUGIN_NAME "orthanc-saola-api"

static const char *const DATABASE = "Database";
static const char *const ORTHANC_STORAGE = "OrthancStorage";
static const char *const STORAGE_DIRECTORY = "StorageDirectory";

static OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                               OrthancPluginResourceType resourceType,
                                               const char *resourceId)
{
  switch (changeType)
  {
  case OrthancPluginChangeType_OrthancStarted:
  {
    StableEventScheduler::Instance().Start();
  }
  break;

  case OrthancPluginChangeType_OrthancStopped:
  {
    StableEventScheduler::Instance().Stop();
  }
  break;

  default:
    break;
  }

  return OrthancPluginErrorCode_Success;
}

extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext *context)
  {
    OrthancPlugins::SetGlobalContext(context);
    Orthanc::Logging::InitializePluginContext(context);
    Orthanc::Logging::EnableInfoLevel(true);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPlugins::SetDescription(ORTHANC_PLUGIN_NAME, "Saola Backend");

    if (!SaolaConfiguration::Instance().IsEnabled())
    {
      OrthancPlugins::LogWarning("OrthancSaola is disabled");
      return 0;
    }

    try
    {
      OrthancPlugins::OrthancConfiguration configuration;
      std::string folder = configuration.GetStringValue(STORAGE_DIRECTORY, ORTHANC_STORAGE);
      ;
      Orthanc::SystemToolbox::MakeDirectory(folder);
      std::string path = (boost::filesystem::path(folder) / "saola-plugin.db").string();

      LOG(WARNING) << "Path to the database of the Saola plugin: " << path;
      SaolaDatabase::Instance().Open(path);

      OrthancPlugins::RegisterRestCallback<HandleStableEvents>(SaolaConfiguration::Instance().GetRoot() + "event-queues", true);
      OrthancPlugins::RegisterRestCallback<DeleteOrResetStableEvent>(SaolaConfiguration::Instance().GetRoot() + "event-queues/([^/]*)", true);
      OrthancPlugins::RegisterRestCallback<UpdateTransferJobs>(SaolaConfiguration::Instance().GetRoot() + "transfer-jobs/([^/]*)/([^/]*)", true);

      OrthancPluginRegisterOnChangeCallback(context, OnChangeCallback);
    }
    catch (Orthanc::OrthancException &e)
    {
      return -1;
    }
    catch (...)
    {
      LOG(ERROR) << "Native exception while initializing the plugin";
      return -1;
    }

    return 0;
  }

  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("Saola plugin is finalizing");
  }

  ORTHANC_PLUGINS_API const char *OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }

  ORTHANC_PLUGINS_API const char *OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
