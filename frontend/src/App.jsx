import React, { useState, useEffect, useRef } from 'react';
import Dashboard from './components/Dashboard';

const WS_URL = `ws://${window.location.hostname}:3000`;

export default function App() {
  const [readings,   setReadings]   = useState([]);
  const [connected,  setConnected]  = useState(false);
  const wsRef = useRef(null);

  useEffect(() => {
    function connect() {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        console.log('[WS] Conectado');
      };

      ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);

        if (msg.type === 'history') {
          setReadings(msg.data);
        } else if (msg.type === 'reading') {
          setReadings((prev) => {
            const updated = [...prev, msg.data];
            // Mantém no máximo 500 leituras em memória
            return updated.length > 500 ? updated.slice(-500) : updated;
          });
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

  return <Dashboard readings={readings} connected={connected} />;
}
