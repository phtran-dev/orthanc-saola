CREATE TABLE AppConfiguration(
       Id TEXT PRIMARY KEY NOT NULL,
       Enable TEXT NOT NULL,
       Type VARCHAR(20),
       Delay INTEGER NOT NULL DEFAULT 0,
       Url TEXT,
       Authentication TEXT,
       Method VARCHAR(10),
       Timeout INTEGER NOT NULL DEFAULT 60,
       FieldMappingOverwrite BOOLEAN DEFAULT false,
       FieldMapping TEXT
       FieldValues TEXT,
       LuaCallback TEXT
       );