const Database = require('better-sqlite3');
const path = require('path');

const db = new Database(path.join(__dirname, 'datalogger.sqlite'));

// Schema trifásico — sistema solar 75kW
db.exec(`
  CREATE TABLE IF NOT EXISTS readings (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    v1         REAL NOT NULL,
    v2         REAL NOT NULL,
    v3         REAL NOT NULL,
    i1         REAL NOT NULL,
    i2         REAL NOT NULL,
    i3         REAL NOT NULL,
    p1         REAL NOT NULL,
    p2         REAL NOT NULL,
    p3         REAL NOT NULL,
    p_total    REAL NOT NULL,
    energy_kwh REAL NOT NULL DEFAULT 0,
    temp       REAL NOT NULL,
    humidity   REAL NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
  )
`);

// Insere uma leitura trifásica completa
const insertReading = db.prepare(`
  INSERT INTO readings (v1, v2, v3, i1, i2, i3, p1, p2, p3, p_total, energy_kwh, temp, humidity)
  VALUES (@v1, @v2, @v3, @i1, @i2, @i3, @p1, @p2, @p3, @p_total, @energy_kwh, @temp, @humidity)
`);

// Retorna as N leituras mais recentes
const getReadings = db.prepare(`
  SELECT * FROM readings
  ORDER BY id DESC
  LIMIT ?
`);

// Energia total acumulada no banco (máximo do acumulador)
const getEnergyTotal = db.prepare(`
  SELECT COALESCE(MAX(energy_kwh), 0) AS total FROM readings
`);

// Resumo diário: data, pico de potência e energia estimada do dia
// Cada linha representa READ_INTERVAL = 5s → 5/3600 horas
const getDailySummary = db.prepare(`
  SELECT
    date(created_at)                           AS day,
    MAX(p_total)                               AS peak_kva,
    COUNT(*)                                   AS count,
    ROUND(SUM(p_total) * 5.0 / 3600.0, 3)    AS energy_kwh_day
  FROM readings
  GROUP BY date(created_at)
  ORDER BY day DESC
  LIMIT 30
`);

module.exports = {
  insertReading,
  getReadings,
  getEnergyTotal,
  getDailySummary,
};
