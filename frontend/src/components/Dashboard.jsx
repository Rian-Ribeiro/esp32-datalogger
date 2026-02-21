import React from 'react';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid,
  Tooltip, Legend, ResponsiveContainer,
} from 'recharts';

// Paleta de cores
const C = {
  L1:     '#f39c12',
  L2:     '#3498db',
  L3:     '#2ecc71',
  pow:    '#a855f7',
  temp:   '#e74c3c',
  hum:    '#3498db',
  bg:     '#12122a',
  border: '#1e1e3a',
};

// ── Componentes base ──────────────────────────────────────────

function StatCard({ label, value, unit, color }) {
  return (
    <div style={{
      background: C.bg,
      border: `1px solid ${color}40`,
      borderRadius: 10,
      padding: '12px 16px',
    }}>
      <div style={{ color: '#666', fontSize: 10, textTransform: 'uppercase', letterSpacing: '.6px', marginBottom: 4 }}>
        {label}
      </div>
      <div style={{ color, fontSize: 30, fontWeight: 700, fontFamily: 'monospace', lineHeight: 1 }}>
        {value !== null && value !== undefined ? value : '—'}
      </div>
      <div style={{ color: '#555', fontSize: 11, marginTop: 3 }}>{unit}</div>
    </div>
  );
}

// Rótulos legíveis para os códigos de alerta
const ALERT_LABELS = {
  OVERCURRENT:  'SOBRECORRENTE',
  UNDERVOLTAGE: 'SUBTENSÃO',
  OVERVOLTAGE:  'SOBRETENSÃO',
  IMBALANCE:    'DESEQUILÍBRIO',
};

function AlertBanner({ alert, onDismiss }) {
  const label = ALERT_LABELS[alert.code] || alert.code;
  const unit  = alert.code === 'IMBALANCE'          ? '%'
              : alert.code.includes('VOLT')          ? 'V' : 'A';
  return (
    <div style={{
      background: '#1a0a0a',
      border: '2px solid #e74c3c',
      borderRadius: 8,
      padding: '10px 14px',
      marginBottom: 6,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
    }}>
      <span style={{ color: '#e74c3c', fontWeight: 600, fontSize: 13 }}>
        ⚠ {label} — Fase {alert.phase} — {Number(alert.value).toFixed(2)}{unit}
      </span>
      <button
        onClick={onDismiss}
        style={{ background: 'none', border: 'none', color: '#e74c3c', cursor: 'pointer', fontSize: 18, lineHeight: 1 }}
      >
        ×
      </button>
    </div>
  );
}

// Gráfico de linha com múltiplas séries
function PhaseChart({ data, lines, title, height = 220 }) {
  return (
    <div style={{ marginBottom: 24 }}>
      <h2 style={{ fontSize: 11, color: '#555', textTransform: 'uppercase', letterSpacing: 1, margin: '0 0 8px', fontWeight: 500 }}>
        {title}
      </h2>
      <ResponsiveContainer width="100%" height={height}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="#1a1a30" />
          <XAxis dataKey="time" tick={{ fontSize: 10, fill: '#555' }} interval="preserveStartEnd" />
          <YAxis tick={{ fontSize: 10, fill: '#555' }} />
          <Tooltip contentStyle={{ background: C.bg, border: `1px solid ${C.border}`, fontSize: 12 }} />
          <Legend wrapperStyle={{ fontSize: 11 }} />
          {lines.map((l) => (
            <Line
              key={l.key}
              type="monotone"
              dataKey={l.key}
              name={l.name}
              stroke={l.color}
              dot={false}
              strokeWidth={2}
            />
          ))}
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}

// ── Dashboard principal ───────────────────────────────────────
export default function Dashboard({ readings, connected, energyKwh, alerts = [], onDismissAlert }) {
  const latest = readings.length > 0 ? readings[readings.length - 1] : null;

  // Normaliza nomes de campo — backend usa p_total, ESP32 usa pt
  const pt  = latest ? (latest.p_total ?? latest.pt ?? 0) : null;
  const gen = pt !== null && pt > 0.5;

  // Últimas 60 leituras para os gráficos
  const chartData = readings.slice(-60).map((r) => ({
    ...r,
    time:    formatTime(r.created_at),
    p_total: r.p_total ?? r.pt ?? 0,
    temp:    r.temp ?? r.t ?? 0,
    humidity: r.humidity ?? r.h ?? 0,
  }));

  return (
    <div style={{
      fontFamily: 'system-ui, sans-serif',
      background: '#0d0d1a',
      minHeight: '100vh',
      color: '#ccc',
      padding: 20,
      maxWidth: 960,
      margin: '0 auto',
    }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 20, paddingBottom: 14, borderBottom: '1px solid #1e1e35' }}>
        <div style={{
          width: 9, height: 9, borderRadius: '50%', flexShrink: 0,
          background: connected ? '#2ecc71' : '#e74c3c',
        }} />
        <h1 style={{ fontSize: 18, color: '#fff', fontWeight: 600, margin: 0 }}>
          ESP32 Datalogger — Sistema Trifásico
        </h1>
        <span style={{
          marginLeft: 'auto', fontSize: 11, padding: '2px 10px', borderRadius: 12,
          background: connected ? '#1a4a2e' : '#4a1a1a',
          color: connected ? '#2ecc71' : '#e74c3c',
        }}>
          {connected ? 'conectado' : 'desconectado'}
        </span>
      </div>

      {/* Alertas */}
      {alerts.length > 0 && (
        <div style={{ marginBottom: 16 }}>
          {alerts.map((a) => (
            <AlertBanner key={a.id} alert={a} onDismiss={() => onDismissAlert(a.id)} />
          ))}
        </div>
      )}

      {/* Card potência total */}
      <div style={{
        background: C.bg, border: `2px solid ${C.pow}`,
        borderRadius: 12, padding: '16px 22px',
        display: 'flex', alignItems: 'center', gap: 24, marginBottom: 14,
      }}>
        <div>
          <div style={{ color: '#666', fontSize: 10, textTransform: 'uppercase', letterSpacing: '.6px', marginBottom: 4 }}>
            Potência Total
          </div>
          <div style={{ color: C.pow, fontSize: 52, fontWeight: 700, fontFamily: 'monospace', lineHeight: 1 }}>
            {pt !== null ? pt.toFixed(2) : '—'}
          </div>
          <div style={{ color: '#555', fontSize: 11, marginTop: 3 }}>kVA</div>
        </div>
        <div>
          <div style={{
            display: 'inline-block', padding: '4px 14px', borderRadius: 20,
            fontSize: 12, fontWeight: 600, letterSpacing: '.5px',
            background: gen ? '#1a3a1f' : '#2a2a2a',
            color: gen ? '#2ecc71' : '#555',
          }}>
            {gen ? 'GERANDO' : 'AGUARDANDO'}
          </div>
          <div style={{ marginTop: 8, fontSize: 12, color: '#555' }}>
            Energia:{' '}
            <span style={{ color: C.pow, fontWeight: 600 }}>
              {energyKwh !== null && energyKwh !== undefined ? Number(energyKwh).toFixed(2) : '—'}
            </span>{' '}kWh
          </div>
        </div>
      </div>

      {/* Grid trifásico 3×3 */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 8, marginBottom: 14 }}>
        {/* Tensões */}
        <StatCard label="L1 Tensão"   value={latest?.v1?.toFixed(1)}  unit="V"   color={C.L1} />
        <StatCard label="L2 Tensão"   value={latest?.v2?.toFixed(1)}  unit="V"   color={C.L2} />
        <StatCard label="L3 Tensão"   value={latest?.v3?.toFixed(1)}  unit="V"   color={C.L3} />
        {/* Correntes */}
        <StatCard label="L1 Corrente" value={latest?.i1?.toFixed(1)}  unit="A"   color={C.L1} />
        <StatCard label="L2 Corrente" value={latest?.i2?.toFixed(1)}  unit="A"   color={C.L2} />
        <StatCard label="L3 Corrente" value={latest?.i3?.toFixed(1)}  unit="A"   color={C.L3} />
        {/* Potências */}
        <StatCard label="L1 Potência" value={latest?.p1?.toFixed(2)}  unit="kVA" color={C.L1} />
        <StatCard label="L2 Potência" value={latest?.p2?.toFixed(2)}  unit="kVA" color={C.L2} />
        <StatCard label="L3 Potência" value={latest?.p3?.toFixed(2)}  unit="kVA" color={C.L3} />
      </div>

      {/* Cards secundários */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginBottom: 20 }}>
        {/* Energia */}
        <div style={{ background: C.bg, border: `1px solid ${C.pow}30`, borderRadius: 10, padding: '12px 16px' }}>
          <div style={{ color: '#666', fontSize: 10, textTransform: 'uppercase', letterSpacing: '.6px', marginBottom: 4 }}>
            Energia Acumulada
          </div>
          <div style={{ color: C.pow, fontSize: 28, fontWeight: 700, fontFamily: 'monospace' }}>
            {energyKwh !== null && energyKwh !== undefined ? Number(energyKwh).toFixed(2) : '—'}
          </div>
          <div style={{ color: '#555', fontSize: 11, marginTop: 3 }}>kWh</div>
        </div>

        {/* Temperatura/Umidade */}
        <div style={{ background: C.bg, border: `1px solid ${C.border}`, borderRadius: 10, padding: '12px 16px' }}>
          <div style={{ color: '#666', fontSize: 10, textTransform: 'uppercase', letterSpacing: '.6px', marginBottom: 8 }}>
            Temperatura / Umidade
          </div>
          <div style={{ display: 'flex', gap: 20 }}>
            <div>
              <div style={{ color: C.temp, fontSize: 26, fontWeight: 700, fontFamily: 'monospace' }}>
                {latest ? (latest.temp ?? latest.t ?? 0).toFixed(1) : '—'}
              </div>
              <div style={{ color: '#555', fontSize: 11 }}>°C</div>
            </div>
            <div>
              <div style={{ color: C.hum, fontSize: 26, fontWeight: 700, fontFamily: 'monospace' }}>
                {latest ? (latest.humidity ?? latest.h ?? 0).toFixed(1) : '—'}
              </div>
              <div style={{ color: '#555', fontSize: 11 }}>%</div>
            </div>
          </div>
        </div>
      </div>

      {/* Gráficos */}
      <PhaseChart
        title="Tensões AC (V)"
        data={chartData}
        lines={[
          { key: 'v1', name: 'L1 (V)', color: C.L1 },
          { key: 'v2', name: 'L2 (V)', color: C.L2 },
          { key: 'v3', name: 'L3 (V)', color: C.L3 },
        ]}
      />
      <PhaseChart
        title="Correntes AC (A)"
        data={chartData}
        lines={[
          { key: 'i1', name: 'L1 (A)', color: C.L1 },
          { key: 'i2', name: 'L2 (A)', color: C.L2 },
          { key: 'i3', name: 'L3 (A)', color: C.L3 },
        ]}
      />
      <PhaseChart
        title="Potência Total (kVA)"
        data={chartData}
        lines={[{ key: 'p_total', name: 'Total (kVA)', color: C.pow }]}
        height={200}
      />
      <PhaseChart
        title="Temperatura & Umidade"
        data={chartData}
        lines={[
          { key: 'temp',     name: 'Temp (°C)',   color: C.temp },
          { key: 'humidity', name: 'Umidade (%)', color: C.hum },
        ]}
        height={200}
      />

      {/* Rodapé */}
      <div style={{ color: '#444', fontSize: 11, marginTop: 8, textAlign: 'right' }}>
        {readings.length} leituras
        {latest?.created_at && ` · última: ${formatTime(latest.created_at)}`}
        {energyKwh !== null && energyKwh !== undefined && ` · energia: ${Number(energyKwh).toFixed(2)} kWh`}
      </div>
    </div>
  );
}

function formatTime(iso) {
  if (!iso) return '';
  return new Date(iso).toLocaleTimeString('pt-BR');
}
