import React from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts';

const COLORS = {
  temp:     '#e74c3c',
  humidity: '#3498db',
  voltage:  '#f39c12',
  current:  '#2ecc71',
};

function StatCard({ label, value, unit, color }) {
  return (
    <div style={{
      background: '#1e1e2e',
      border: `2px solid ${color}`,
      borderRadius: 8,
      padding: '20px 28px',
      minWidth: 160,
      textAlign: 'center',
    }}>
      <div style={{ color: '#aaa', fontSize: 13, marginBottom: 6 }}>{label}</div>
      <div style={{ color, fontSize: 36, fontWeight: 700, fontFamily: 'monospace' }}>
        {value !== null ? value : '—'}
      </div>
      <div style={{ color: '#888', fontSize: 12, marginTop: 4 }}>{unit}</div>
    </div>
  );
}

function formatTime(iso) {
  if (!iso) return '';
  const d = new Date(iso);
  return d.toLocaleTimeString('pt-BR');
}

export default function Dashboard({ readings, connected }) {
  const latest = readings.length > 0 ? readings[readings.length - 1] : null;

  // Últimas 60 leituras para os gráficos
  const chartData = readings.slice(-60).map((r) => ({
    ...r,
    time: formatTime(r.created_at),
  }));

  return (
    <div style={{ fontFamily: 'sans-serif', background: '#13131f', minHeight: '100vh', color: '#fff', padding: 24 }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 24 }}>
        <h1 style={{ margin: 0, fontSize: 22 }}>ESP32 Datalogger</h1>
        <span style={{
          fontSize: 12,
          padding: '2px 10px',
          borderRadius: 12,
          background: connected ? '#1a4a2e' : '#4a1a1a',
          color: connected ? '#2ecc71' : '#e74c3c',
        }}>
          {connected ? 'conectado' : 'desconectado'}
        </span>
      </div>

      {/* Cards com valores atuais */}
      <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', marginBottom: 36 }}>
        <StatCard
          label="Temperatura"
          value={latest ? latest.temp.toFixed(1) : null}
          unit="°C"
          color={COLORS.temp}
        />
        <StatCard
          label="Umidade"
          value={latest ? latest.humidity.toFixed(1) : null}
          unit="%"
          color={COLORS.humidity}
        />
        <StatCard
          label="Tensão"
          value={latest ? latest.voltage.toFixed(1) : null}
          unit="V AC"
          color={COLORS.voltage}
        />
        <StatCard
          label="Corrente"
          value={latest ? latest.current.toFixed(2) : null}
          unit="A"
          color={COLORS.current}
        />
      </div>

      {/* Gráfico Temperatura + Umidade */}
      <Section title="Temperatura e Umidade">
        <ResponsiveContainer width="100%" height={220}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke="#2a2a3e" />
            <XAxis dataKey="time" tick={{ fontSize: 11, fill: '#888' }} interval="preserveStartEnd" />
            <YAxis tick={{ fontSize: 11, fill: '#888' }} />
            <Tooltip contentStyle={{ background: '#1e1e2e', border: '1px solid #333' }} />
            <Legend />
            <Line type="monotone" dataKey="temp"     name="Temp (°C)"  stroke={COLORS.temp}     dot={false} strokeWidth={2} />
            <Line type="monotone" dataKey="humidity" name="Umidade (%)" stroke={COLORS.humidity} dot={false} strokeWidth={2} />
          </LineChart>
        </ResponsiveContainer>
      </Section>

      {/* Gráfico Tensão */}
      <Section title="Tensão AC">
        <ResponsiveContainer width="100%" height={200}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke="#2a2a3e" />
            <XAxis dataKey="time" tick={{ fontSize: 11, fill: '#888' }} interval="preserveStartEnd" />
            <YAxis tick={{ fontSize: 11, fill: '#888' }} />
            <Tooltip contentStyle={{ background: '#1e1e2e', border: '1px solid #333' }} />
            <Line type="monotone" dataKey="voltage" name="Tensão (V)" stroke={COLORS.voltage} dot={false} strokeWidth={2} />
          </LineChart>
        </ResponsiveContainer>
      </Section>

      {/* Gráfico Corrente */}
      <Section title="Corrente AC">
        <ResponsiveContainer width="100%" height={200}>
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke="#2a2a3e" />
            <XAxis dataKey="time" tick={{ fontSize: 11, fill: '#888' }} interval="preserveStartEnd" />
            <YAxis tick={{ fontSize: 11, fill: '#888' }} />
            <Tooltip contentStyle={{ background: '#1e1e2e', border: '1px solid #333' }} />
            <Line type="monotone" dataKey="current" name="Corrente (A)" stroke={COLORS.current} dot={false} strokeWidth={2} />
          </LineChart>
        </ResponsiveContainer>
      </Section>

      <div style={{ color: '#555', fontSize: 12, marginTop: 16, textAlign: 'right' }}>
        {readings.length} leituras carregadas
        {latest && ` · última: ${formatTime(latest.created_at)}`}
      </div>
    </div>
  );
}

function Section({ title, children }) {
  return (
    <div style={{ marginBottom: 24 }}>
      <h2 style={{ fontSize: 14, color: '#aaa', margin: '0 0 10px', fontWeight: 500, textTransform: 'uppercase', letterSpacing: 1 }}>
        {title}
      </h2>
      {children}
    </div>
  );
}
