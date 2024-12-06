#pragma once

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"


void GetStableEvents(OrthancPluginRestOutput *output,
                     const char *url,
                     const OrthancPluginHttpRequest *request);

void SaveStableEvent(OrthancPluginRestOutput *output,
                     const char *url,
                     const OrthancPluginHttpRequest *request);

void DeleteStableEvent(OrthancPluginRestOutput *output,
                       const char *url,
                       const OrthancPluginHttpRequest *request);

void SaveFailedJob(OrthancPluginRestOutput *output,
                   const char *url,
                   const OrthancPluginHttpRequest *request);

void ResetFailedJob(OrthancPluginRestOutput *output,
                    const char *url,
                    const OrthancPluginHttpRequest *request);

void GetFailedJobs(OrthancPluginRestOutput *output,
                   const char *url,
                   const OrthancPluginHttpRequest *request);

void DeleteFailedJob(OrthancPluginRestOutput *output,
                     const char *url,
                     const OrthancPluginHttpRequest *request);

void HandleFailedJobService(OrthancPluginRestOutput *output,
                            const char *url,
                            const OrthancPluginHttpRequest *request);