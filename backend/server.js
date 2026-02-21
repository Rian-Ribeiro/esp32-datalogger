const express = require('express');
const cors    = require('cors');
const http    = require('http');
const { WebSocketServer } = require('ws');
const { insertReading, getReadings, getEnergyTotal, getDailySummary } = require('./db');

const PORT = process.env.PORT || 3000;

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocketServer({ server });

// CORS liberado — permite acesso a partir do origin do ESP32 ou qualquer origin
app.use(cors());
app.use(express.json());

// ── WebSocket ─────────────────────────────────────────────────
function broadcast(data) {
  const msg = JSON.stringify(data);
  wss.clients.forEach((client) => {
    if (client.readyState === client.OPEN) {
      client.send(msg);
    }
  });
}

wss.on('connection', (ws) => {
  console.log('[WS] Cliente conectado');

  // Envia últimas 50 leituras ao novo cliente
  const recent = getReadings.all(50).reverse();
  const energy = getEnergyTotal.get();
  ws.send(JSON.stringify({
    type: 'history',
    kwh:  energy.total,
    data: recent,
  }));

  ws.on('close', () => console.log('[WS] Cliente desconectado'));
});

// ── REST ─────────────────────────────────────────────────────

// POST /api/data — recebe leitura trifásica do ESP32 ou bridge
app.post('/api/data', (req, res) => {
  const { v1, v2, v3, i1, i2, i3, p1, p2, p3, p_total, energy_kwh, temp, humidity } = req.body;

  // Validação de campos obrigatórios
  const required = { v1, v2, v3, i1, i2, i3, p1, p2, p3, p_total, temp, humidity };
  const missing  = Object.keys(required).filter((k) => required[k] === undefined);
  if (missing.length > 0) {
    return res.status(400).json({
      error: 'Campos obrigatórios ausentes: ' + missing.join(', '),
    });
  }

  const row = {
    v1:         parseFloat(v1),
    v2:         parseFloat(v2),
    v3:         parseFloat(v3),
    i1:         parseFloat(i1),
    i2:         parseFloat(i2),
    i3:         parseFloat(i3),
    p1:         parseFloat(p1),
    p2:         parseFloat(p2),
    p3:         parseFloat(p3),
    p_total:    parseFloat(p_total),
    energy_kwh: parseFloat(energy_kwh || 0),
    temp:       parseFloat(temp),
    humidity:   parseFloat(humidity),
  };

  const result  = insertReading.run(row);
  const reading = { id: result.lastInsertRowid, ...row, created_at: new Date().toISOString() };

  console.log(`[POST] P=${row.p_total.toFixed(2)}kVA T=${row.temp}°C`);

  // Repassa para todos os clientes WebSocket (incluindo alertas se presentes)
  broadcast({ type: 'reading', data: reading });

  res.status(201).json(reading);
});

// GET /api/data?limit=N — retorna últimas N leituras (default 100)
app.get('/api/data', (req, res) => {
  const limit = Math.min(parseInt(req.query.limit) || 100, 1000);
  const rows  = getReadings.all(limit).reverse();
  res.json(rows);
});

// GET /api/energy — energia total acumulada no banco
app.get('/api/energy', (req, res) => {
  const result = getEnergyTotal.get();
  res.json({ kwh: result.total });
});

// GET /api/daily — resumo diário (últimos 30 dias)
app.get('/api/daily', (req, res) => {
  const rows = getDailySummary.all();
  res.json(rows);
});

// Repassa alertas recebidos via POST para os clientes WebSocket
// (útil quando um bridge/script envia alertas do ESP32 standalone)
app.post('/api/alert', (req, res) => {
  const { code, phase, value } = req.body;
  if (!code || !phase || value === undefined) {
    return res.status(400).json({ error: 'Campos obrigatórios: code, phase, value' });
  }
  broadcast({ type: 'alert', code, phase, value: parseFloat(value) });
  res.json({ ok: true });
});

server.listen(PORT, () => {
  console.log(`[Server] Rodando em http://localhost:${PORT}`);
  console.log(`[Server] WebSocket em ws://localhost:${PORT}`);
});
