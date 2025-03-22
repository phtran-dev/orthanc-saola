cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Debug


      "{
        "Id": "Ris1",
        "Enable": true,
        "FieldMappingOverwrite": false,
        "Type": "Ris",
        "Delay": 60, // Delay in seconds
        "Url": "http://storeserver:9001/secured/ws/rest/v1/async/location",
        "Authentication": "Basic b3J0aGFuYzpvcnRoYW5j",
        "FieldMapping": ["aeTitle:RemoteAET", "ipAddress:RemoteIP"]
      }


INSERT INTO AppConfiguration ("Id", "Enable", "FieldMappingOverwrite", "Type", "Delay", "Url", "Authentication", "FieldMapping") VALUES ('Ris2', 'true', 0, 'Ris', 60, 'http://storeserver:9001/secured/ws/rest/v1/async/location', 'Basic b3J0aGFuYzpvcnRoYW5j', '[{"aeTitle1": "RemoteAET", "ipAddress1": "RemoteIP"}]');

INSERT INTO AppConfiguration ("Id", "Enable", "FieldMappingOverwrite", "Type", "Delay", "Url", "Authentication", "FieldMapping") VALUES ('Ris3', 'true', 0, 'Ris', 60, 'http://storeserver:9001/secured/ws/rest/v1/async/location', 'Basic b3J0aGFuYzpvcnRoYW5j', '[{"aeTitle2": "RemoteAET", "ipAddress2": "RemoteIP"}]');


localhost:8042/itech/orthanc-saola/configuration