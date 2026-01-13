CREATE TABLE StableEventQueues(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  status VARCHAR(20) DEFAULT 'PENDING',
  owner_id VARCHAR(100),
  patient_birth_date TEXT,
  patient_id TEXT,
  patient_name TEXT,
  patient_sex TEXT,
  accession_number TEXT,
  iuid TEXT NOT NULL,
  resource_id TEXT NOT NULL,
  resource_type VARCHAR(10) NOT NULL,
  app_id TEXT NOT NULL,
  app_type VARCHAR(30) NOT NULL,
  delay_sec INTEGER DEFAULT 0,
  retry INTEGER DEFAULT 0,
  failed_reason TEXT,
  next_scheduled_time TEXT,
  expiration_time TEXT,
  last_updated_time TEXT,
  creation_time TEXT
);

CREATE TABLE TransferJobs(
  id TEXT PRIMARY KEY,
  queue_id INTEGER REFERENCES StableEventQueues(id),
  last_updated_time TEXT,
  creation_time TEXT
);


