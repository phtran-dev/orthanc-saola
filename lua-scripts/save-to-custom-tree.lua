PEER_URL = 'http://192.168.1.40:9045/studies/'



PEER_HEADER = {
    ["Authorization"] = "Basic b3J0aGFuYzpvcnRoYW5j",
 }

UPLOAD_DIR = '/tmp/upload-dicom'

SAOLA_URL = '/itech/'

function AddToEventQueue(studyId, tags, metadata)
  ------------------------------------------------
  print('[AddToEventQueue] Add stable study ' .. studyId )
  local body = {}
  body["iuid"] = tags["StudyInstanceUID"]
  body["resource_id"] = studyId
  body["resource_type"] = "study"


  -- DumpJson false means it does not interprete number to string
  -- https://groups.google.com/g/orthanc-users/c/hmv2y-LgKm8/m/oMAuGJWMBgAJ
  -- RestApiPost(SAOLA_URL .. 'event-queues' , DumpJson(body, false))

  -- Send dicom to RIS
  body["app"] = "Ris1"
  PrintRecursive(body)
  RestApiPost(SAOLA_URL .. 'event-queues' , DumpJson(body, false))

  -- Send metadata to store server
  body["app"] = "StoreServer1"
  PrintRecursive(body)
  RestApiPost(SAOLA_URL .. 'event-queues' , DumpJson(body, false))

  -- Send metadata to exporter
  body["app"] = "Transfer1"
  PrintRecursive(body)
  RestApiPost(SAOLA_URL .. 'event-queues' , DumpJson(body, false))
end


function CompareStudyInstances(studyId, tags)
    my_instances = ParseJson(RestApiGet('/studies/' .. studyId .. '/statistics'))['CountInstances']
    print("Study " .. studyId .. ", StudyInstanceUID " .. tags["StudyInstanceUID"] .. " has " .. my_instances .. " instances")
    local rawResponse = HttpGet(PEER_URL .. studyId .. '/statistics', PEER_HEADER)
    if rawResponse ~= nil and rawResponse ~= '' then
        -- transform the response into a lua table for internal processing
        local response = ParseJson(rawResponse)
        peer_instances = response['CountInstances']
        print("SUCCESS: PEER " .. PEER_URL .. studyId .. ", StudyInstanceUID " .. tags["StudyInstanceUID"] .. " has " .. peer_instances .. " instances"))
        if my_instances == peer_instances then
            return true
        end
    else
        print("ERROR: PEER " .. PEER_URL .. " , cannot query studyId " .. studyId .. ", StudyInstanceUID " .. tags["StudyInstanceUID"]))
    end
    return false
end

function OnStoredInstance(instanceId, tags, metadata, origin)
    -- store files in a more human friendly hierarchy and then, delete the instance from Orthanc
 
    local studyId = tags["StudyInstanceUID"]
    local seriesId = tags["SeriesInstanceUID"]
    local sopInstanceId = tags["SOPInstanceUID"]
 
    local dicom = RestApiGet('/instances/' .. instanceId .. '/file')
    local folder = UPLOAD_DIR .. '/' .. studyId .. '/' .. seriesId 
    print(folder)

    os.execute("mkdir -p " .. folder)
    local path = folder .. '/' .. sopInstanceId .. ".dcm" 
    local file = io.open(path, "wb")
    io.output(file)
    io.write(dicom)
    io.close(file)
end

function OnStableStudy(studyId, tags, metadata)
    print('[OnStableStudy]: ' .. studyId .. ' begin tags')
    PrintRecursive(tags)
    PrintRecursive(metadata)
    print('[OnStableStudy]: ' .. studyId .. ' end tags')

    if not CompareStudyInstances(studyId, tags) then
        AddToEventQueue(studyId, tags, metadata)
    end
end
