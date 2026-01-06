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


curl  -X POST \
  'http://localhost:8042/itech/orthanc-saola/configuration/apply' \
  --header 'Accept: */*' \
  --header 'User-Agent: Thunder Client (https://www.thunderclient.com)' \
  --header 'Authorization: Basic b3J0aGFuYzpvcnRoYW5j' \
  --header 'Content-Type: application/json' \
  --data-raw '[
  {
    "Id": "Ris1",
    "Enable": true,
    "FieldMappingOverwrite": false,
    "Type": "Ris",
    "Delay": 60,
    "Url": "http://storeserver:9001/secured/ws/rest/v1/async/location",
    "Authentication": "Basic b3J0aGFuYzpvcnRoYW5j",
    "FieldMapping": [
      {
        "aeTitle": "RemoteAET"
      },
      {
        "ipAddress": "RemoteIP"
      }
    ],
    "FieldValues": [
      {
        "Peer": "LongTermPeer"
      },
      {
        "Compression": "none"
      }
    ]
  },
  {
    "Id": "Transfer1",
    "Enable": true,
    "FieldMappingOverwrite": true,
    "Type": "Transfer",
    "Delay": 200,
    "Url": "/transfers/send",
    "Method": "POST",
    "FieldValues": [
      {
        "Peer": "LongTermPeer"
      },
      {
        "Compression": "none"
      }
    ]
  },
  {
    "Id": "Exporter1",
    "Enable": true,
    "FieldMappingOverwrite": true,
    "Type": "Exporter",
    "Delay": 1,
    "Url": "/itech/export",
    "Method": "POST",
    "FieldValues": [
      {
        "ExportDir": "/upload2"
      },
      {
        "RetentionExpired": 24
      }
    ]
  }
]'


RQLite
docker run -p4001:4001 --rm  -v $(pwd)/auth_config.json:/rqlite/auth_config.json rqlite/rqlite  -http-allow-origin "*" 
$ cat auth_config.json 
[
  {
    "username": "username",
    "password": "password",
    "perms": ["all"]
  },
  {
    "username": "mary",
    "password": "secret2",
    "perms": ["query", "backup", "join"]
  },
  {
    "username": "*",
    "perms": ["status", "ready", "join-read-only"]
  }
]

Studio
https://github.com/outerbase/studio

Example of how to use exporter api 
curl  -X POST \
  'localhost:8042/itech/export' \
  --header 'Accept: */*' \
  --header 'User-Agent: Thunder Client (https://www.thunderclient.com)' \
  --header 'Authorization: Basic ZGVtbzpkZW1v' \
  --header 'Content-Type: application/json' \
  --data-raw '{
  "ExportDir": "/tmp/abc1",
  "Level": "Study",
  "StudyInstanceUID": "2.25.63750058429518346213142803415976630878",
  "ThreadCount": 8,
  "Asynchronous": true,
  "UseJobEngine": true,
  "Transcode": "1.2.840.10008.1.2.4.80"
}'
