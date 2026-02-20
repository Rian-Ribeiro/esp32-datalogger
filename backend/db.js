const Database = require('better-sqlite3');
const path = require('path');

const db = new Database(path.join(__dirname, 'datalogger.sqlite'));

// Cria tabela se não existir
db.exec(`
  CREATE TABLE IF NOT EXISTS readings (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    temp       REAL    NOT NULL,
    humidity   REAL    NOT NULL,
    voltage    REAL    NOT NULL,
    current    REAL    NOT NULL,
    created_at TEXT    NOT NULL DEFAULT (datetime('now'))
  )
`);

const insertReading = db.prepare(`
  INSERT INTO readings (temp, humidity, voltage, current)
  VALUES (@temp, @humidity, @voltage, @current)
`);

const getReadings = db.prepare(`
  SELECT * FROM readings
  ORDER BY id DESC
  LIMIT ?
`);

module.exports = {
  insertReading,
  getReadings,
};
