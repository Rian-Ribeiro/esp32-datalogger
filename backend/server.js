const express = require('express');
const cors    = require('cors');
const http    = require('http');
const { WebSocketServer } = require('ws');
const { insertReading, getReadings } = require('./db');

const PORT = process.env.PORT || 3000;

const app    = express();
const server = http.createServer(app);
const wss    = new WebSocketServer({ server });

app.use(cors());
app.use(express.json());

// --- WebSocket ---
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
  ws.send(JSON.stringify({ type: 'history', data: recent }));

  ws.on('close', () => console.log('[WS] Cliente desconectado'));
});

// --- REST ---

// POST /api/data — recebe leitura do ESP32
app.post('/api/data', (req, res) => {
  const { temp, humidity, voltage, current } = req.body;

  if (temp === undefined || humidity === undefined ||
      voltage === undefined || current === undefined) {
    return res.status(400).json({ error: 'Campos obrigatórios: temp, humidity, voltage, current' });
  }

  const row = {
    temp:     parseFloat(temp),
    humidity: parseFloat(humidity),
    voltage:  parseFloat(voltage),
    current:  parseFloat(current),
  };

  const result = insertReading.run(row);
  const reading = { id: result.lastInsertRowid, ...row, created_at: new Date().toISOString() };

  console.log(`[POST] ${JSON.stringify(reading)}`);

  // Broadcast para todos os clientes WebSocket
  broadcast({ type: 'reading', data: reading });

  res.status(201).json(reading);
});

// GET /api/data?limit=N — retorna últimas N leituras (default 100)
app.get('/api/data', (req, res) => {
  const limit = parseInt(req.query.limit) || 100;
  const rows  = getReadings.all(limit).reverse();
  res.json(rows);
});

server.listen(PORT, () => {
  console.log(`[Server] Rodando em http://localhost:${PORT}`);
  console.log(`[Server] WebSocket em ws://localhost:${PORT}`);
});
