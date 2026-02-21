import React, { useState, useEffect, useRef } from 'react';
import Dashboard from './components/Dashboard';

// WS_URL configurável via variável de ambiente Vite.
// Defina VITE_WS_URL no .env para apontar para outro host/porta.
// Padrão: conecta ao backend na porta 3000 do mesmo hostname.
const WS_URL = import.meta.env.VITE_WS_URL || `ws://${window.location.hostname}:3000`;

export default function App() {
  const [readings,   setReadings]   = useState([]);
  const [connected,  setConnected]  = useState(false);
  const [energyKwh,  setEnergyKwh]  = useState(null);
  const [alerts,     setAlerts]     = useState([]);
  const wsRef = useRef(null);

  useEffect(() => {
    function connect() {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        console.log('[WS] Conectado a', WS_URL);
      };

      ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);

        if (msg.type === 'history') {
          setReadings(msg.data || []);
          if (msg.kwh !== undefined) setEnergyKwh(msg.kwh);

        } else if (msg.type === 'reading') {
          // Normaliza campo: backend usa 'data', ESP32 usa campos diretos
          const r = msg.data || msg;
          setReadings((prev) => {
            const updated = [...prev, r];
            return updated.length > 500 ? updated.slice(-500) : updated;
          });
          if (r.energy_kwh !== undefined) setEnergyKwh(r.energy_kwh);

        } else if (msg.type === 'alert') {
          const alert = { ...msg, id: Date.now() };
          setAlerts((prev) => [...prev.slice(-4), alert]); // mantém até 5 alertas
          // Remove após 30s
          setTimeout(() => {
            setAlerts((prev) => prev.filter((a) => a.id !== alert.id));
          }, 30000);
        }
      };

      ws.onclose = () => {
        setConnected(false);
        console.log('[WS] Desconectado — reconectando em 3s...');
        setTimeout(connect, 3000);
      };

      ws.onerror = (err) => {
        console.error('[WS] Erro:', err);
        ws.close();
      };
    }

    connect();

    return () => {
      if (wsRef.current) wsRef.current.close();
    };
  }, []);

  function dismissAlert(id) {
    setAlerts((prev) => prev.filter((a) => a.id !== id));
  }

  return (
    <Dashboard
      readings={readings}
      connected={connected}
      energyKwh={energyKwh}
      alerts={alerts}
      onDismissAlert={dismissAlert}
    />
  );
}
