CREATE TABLE StableEventQueues(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  iuid TEXT NOT NULL,
  resource_id TEXT NOT NULL,
  resource_type VARCHAR(10) NOT NULL,
  app_id TEXT NOT NULL,
  app_type VARCHAR(30) NOT NULL,
  delay_sec INTEGER DEFAULT 0,
  retry INTEGER DEFAULT 0,
  failed_reason TEXT,
  creation_time TEXT
);

CREATE TABLE TransferJobs(
  id TEXT PRIMARY KEY,
  queue_id INTEGER REFERENCES StableEventQueues(id),
  last_updated_time TEXT,
  creation_time TEXT
);


