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
  order_type: 'LIMIT' | 'MARKET' | 'STOP' | 'STOP_LIMIT';
  price: number;
  stop_price: number;
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

export interface PositionInfo {
  symbol: string;
  quantity: number;
  avg_entry_price: number;
  last_price: number;
  unrealized_pnl: number;
  realized_pnl: number;
  market_value: number;
}

export interface PnLState {
  initial_capital: number;
  equity: number;
  total_pnl: number;
  realized_pnl: number;
  unrealized_pnl: number;
  positions: PositionInfo[];
  fill_count: number;
}

interface OrderbookState {
  // Auth state
  isAuthenticated: boolean;
  username: string;
  sessionToken: string;
  authError: string;

  connected: boolean;
  bids: PriceLevel[];
  asks: PriceLevel[];
  trades: Trade[];
  priceHistory: PricePoint[];
  orders: Map<string, OrderInfo>;
  notifications: Notification[];
  pnl: PnLState;

  setAuth: (success: boolean, username: string, token: string, error: string) => void;
  logout: () => void;
  setConnected: (v: boolean) => void;
  setSnapshot: (bids: PriceLevel[], asks: PriceLevel[]) => void;
  addTrade: (t: Trade) => void;
  updateOrder: (o: OrderInfo) => void;
  addNotification: (n: Notification) => void;
  removeNotification: (id: string) => void;
  updatePnL: (p: PnLState) => void;
}

const MAX_TRADES = 100;
const MAX_PRICE_HISTORY = 2000;
const MAX_NOTIFICATIONS = 50;

export const useOrderbookStore = create<OrderbookState>((set) => ({
  // Auth state
  isAuthenticated: false,
  username: '',
  sessionToken: '',
  authError: '',

  connected: false,
  bids: [],
  asks: [],
  trades: [],
  priceHistory: [],
  orders: new Map(),
  notifications: [],
  pnl: {
    initial_capital: 100000,
    equity: 100000,
    total_pnl: 0,
    realized_pnl: 0,
    unrealized_pnl: 0,
    positions: [],
    fill_count: 0,
  },

  setAuth: (success, username, token, error) =>
    set({
      isAuthenticated: success,
      username: success ? username : '',
      sessionToken: success ? token : '',
      authError: error,
    }),

  logout: () =>
    set({
      isAuthenticated: false,
      username: '',
      sessionToken: '',
      authError: '',
    }),

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

  updatePnL: (p) => set({ pnl: p }),
}));
