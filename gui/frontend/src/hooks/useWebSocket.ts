/**
 * WebSocket hook — connects to the Python bridge, dispatches messages to store.
 * Reconnects automatically on disconnect.
 */

import { useEffect, useRef, useCallback } from 'react';
import {
  useOrderbookStore,
  PriceLevel,
  Trade,
  OrderInfo,
  Notification,
} from '../store/orderbookStore';

let idCounter = 0;
function nextId(): string {
  return `n-${Date.now()}-${idCounter++}`;
}

export function useWebSocket() {
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const {
    setConnected,
    setSnapshot,
    addTrade,
    updateOrder,
    addNotification,
  } = useOrderbookStore();

  const connect = useCallback(() => {
    // In dev, Vite proxy rewrites /ws → ws://localhost:3001/ws
    const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const url =
      import.meta.env.DEV
        ? `${proto}://${window.location.host}/ws`
        : `${proto}://${window.location.host}/ws`;

    const ws = new WebSocket(url);
    wsRef.current = ws;

    ws.onopen = () => {
      setConnected(true);
      addNotification({
        id: nextId(),
        message: 'Connected to server',
        type: 'success',
        time: Date.now(),
      });
    };

    ws.onclose = () => {
      setConnected(false);
      addNotification({
        id: nextId(),
        message: 'Disconnected — reconnecting…',
        type: 'error',
        time: Date.now(),
      });
      scheduleReconnect();
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        handleMessage(msg);
      } catch {
        // ignore malformed messages
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const scheduleReconnect = useCallback(() => {
    if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
    reconnectTimer.current = setTimeout(() => {
      connect();
    }, 2000);
  }, [connect]);

  const handleMessage = useCallback(
    (msg: Record<string, unknown>) => {
      const type = msg.type as string;

      switch (type) {
        case 'ORDERBOOK_SNAPSHOT': {
          const bids = (msg.bids as PriceLevel[]) ?? [];
          const asks = (msg.asks as PriceLevel[]) ?? [];
          setSnapshot(bids, asks);
          break;
        }

        case 'TRADE_NOTIFICATION': {
          const t: Trade = {
            buy_order_id: msg.buy_order_id as string,
            sell_order_id: msg.sell_order_id as string,
            symbol: msg.symbol as string,
            price: msg.price as number,
            quantity: msg.quantity as number,
            timestamp: msg.timestamp as string,
            receivedAt: Date.now(),
          };
          addTrade(t);
          addNotification({
            id: nextId(),
            message: `Trade: ${t.quantity}@${t.price.toFixed(2)}`,
            type: 'info',
            time: Date.now(),
          });
          break;
        }

        case 'ORDER_STATUS': {
          const o: OrderInfo = {
            order_id: msg.order_id as string,
            client_id: msg.client_id as string,
            symbol: msg.symbol as string,
            side: msg.side as 'BUY' | 'SELL',
            order_type: (msg.order_type as 'LIMIT' | 'MARKET') ?? 'LIMIT',
            price: msg.price as number,
            quantity: msg.quantity as number,
            filled_quantity: msg.filled_quantity as number,
            status: msg.status as string,
            timestamp: msg.timestamp as string,
          };
          updateOrder(o);
          const statusLabel =
            o.status === 'FILLED'
              ? 'filled'
              : o.status === 'CANCELED'
              ? 'canceled'
              : o.status === 'PARTIAL_FILL'
              ? 'partially filled'
              : 'accepted';
          addNotification({
            id: nextId(),
            message: `Order ${o.order_id.slice(0, 8)}… ${statusLabel}`,
            type: o.status === 'CANCELED' ? 'error' : 'success',
            time: Date.now(),
          });
          break;
        }

        case 'ERROR': {
          addNotification({
            id: nextId(),
            message: `Error: ${msg.error_message ?? 'unknown'}`,
            type: 'error',
            time: Date.now(),
          });
          break;
        }

        default:
          break;
      }
    },
    [setSnapshot, addTrade, updateOrder, addNotification],
  );

  /* send json command to bridge */
  const send = useCallback((data: Record<string, unknown>) => {
    const ws = wsRef.current;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(data));
    }
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      wsRef.current?.close();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return { send };
}
