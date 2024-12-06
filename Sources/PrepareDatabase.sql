CREATE TABLE StableEventQueues(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  iuid TEXT NOT NULL,
  resource_id TEXT NOT NULL,
  resource_type VARCHAR(10) NOT NULL,
  app VARCHAR(10) NOT NULL,
  delay_sec INTEGER DEFAULT 0,
  retry INTEGER DEFAULT 0,
  failed_reason TEXT,
  creation_time TEXT
);

CREATE TABLE FailedJobs(
  id TEXT PRIMARY KEY,
  content TEXT,
  retry INTEGER DEFAULT 0,
  last_updated_time TEXT,
  creation_time TEXT
);


