function OnJobFailure(jobId)
    print("PHONG FAILED job " .. jobId .. " failed")
  
    local job = ParseJson(RestApiGet("/jobs/" .. jobId))
    PrintRecursive(job)
  
    if job["Type"] == "PushTransfer" then
      local response = ParseJson(RestApiGet('/saola/transfer-jobs/' .. jobId .. '/failure'))
      PrintRecursive(response)
    end
  end
  
  function OnJobSuccess(jobId)
    print("PHONG SUCCESS job " .. jobId .. " failed")
  
    local job = ParseJson(RestApiGet("/jobs/" .. jobId))
    PrintRecursive(job)
  
    if job["Type"] == "PushTransfer" then
      local response = ParseJson(RestApiGet('/saola/transfer-jobs/' .. jobId .. '/success'))
      PrintRecursive(response)
    end
  
  end
  
  function OnStableSeries(seriesId, tags, metadata)
    ------------------------------------------------
    print('[Transfer DICOM] Send stable series to Peer ' .. seriesId )
    local body = {}
    body["app"] = "Transfer1"
    body["iuid"] = tags["SeriesInstanceUID"]
    body["resource_id"] = seriesId
    body["resource_type"] = "series"
  
    PrintRecursive(body)
    -- DumpJson false means it does not interprete number to string
    -- https://groups.google.com/g/orthanc-users/c/hmv2y-LgKm8/m/oMAuGJWMBgAJ
    RestApiPost('/saola/event-queues' , DumpJson(body, false))
    ------------------------------------------------
  end
  