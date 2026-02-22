/**
 * Zustand store for orderbook state.
 * Holds bids/asks/trades/orders/notifications updated from WebSocket.
 */

import { create } from 'zustand';

export interface PriceLevel {
  price: number;
  quantity: number;
  order_count: number;
}

export interface Trade {
  buy_order_id: string;
  sell_order_id: string;
  symbol: string;
  price: number;
  quantity: number;
  timestamp: string;
  receivedAt: number; // local ms timestamp for display ordering
}

export interface OrderInfo {
  order_id: string;
  client_id: string;
  symbol: string;
  side: 'BUY' | 'SELL';
  order_type: 'LIMIT' | 'MARKET';
  price: number;
  quantity: number;
  filled_quantity: number;
  status: string;
  timestamp: string;
}

export interface Notification {
  id: string;
  message: string;
  type: 'success' | 'error' | 'info';
  time: number;
}

export interface PricePoint {
  price: number;
  quantity: number;
  time: number;
}

interface OrderbookState {
  connected: boolean;
  bids: PriceLevel[];
  asks: PriceLevel[];
  trades: Trade[];
  priceHistory: PricePoint[];
  orders: Map<string, OrderInfo>;
  notifications: Notification[];

  setConnected: (v: boolean) => void;
  setSnapshot: (bids: PriceLevel[], asks: PriceLevel[]) => void;
  addTrade: (t: Trade) => void;
  updateOrder: (o: OrderInfo) => void;
  addNotification: (n: Notification) => void;
  removeNotification: (id: string) => void;
}

const MAX_TRADES = 100;
const MAX_PRICE_HISTORY = 2000;
const MAX_NOTIFICATIONS = 50;

export const useOrderbookStore = create<OrderbookState>((set) => ({
  connected: false,
  bids: [],
  asks: [],
  trades: [],
  priceHistory: [],
  orders: new Map(),
  notifications: [],

  setConnected: (v) => set({ connected: v }),

  setSnapshot: (bids, asks) => set({ bids, asks }),

  addTrade: (t) =>
    set((state) => ({
      trades: [t, ...state.trades].slice(0, MAX_TRADES),
      priceHistory: [
        ...state.priceHistory,
        { price: t.price, quantity: t.quantity, time: t.receivedAt },
      ].slice(-MAX_PRICE_HISTORY),
    })),

  updateOrder: (o) =>
    set((state) => {
      const next = new Map(state.orders);
      next.set(o.order_id, o);
      return { orders: next };
    }),

  addNotification: (n) =>
    set((state) => ({
      notifications: [n, ...state.notifications].slice(0, MAX_NOTIFICATIONS),
    })),

  removeNotification: (id) =>
    set((state) => ({
      notifications: state.notifications.filter((n) => n.id !== id),
    })),
}));
