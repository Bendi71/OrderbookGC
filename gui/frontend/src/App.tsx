/**
 * Root App component — dashboard layout.
 * Left column: OrderbookView + DepthChart
 * Right column: OrderEntry + OrderList + TradeHistory
 * Header: connection status indicator
 */

import React from 'react';
import { useWebSocket } from './hooks/useWebSocket';
import { useOrderbookStore } from './store/orderbookStore';
import OrderbookView from './components/OrderbookView';
import DepthChart from './components/DepthChart';
import PriceChart from './components/PriceChart';
import OrderEntry from './components/OrderEntry';
import OrderList from './components/OrderList';
import TradeHistory from './components/TradeHistory';
import Notifications from './components/Notifications';

const App: React.FC = () => {
  const { send } = useWebSocket();
  const connected = useOrderbookStore((s) => s.connected);

  return (
    <div className="min-h-screen bg-gray-900 text-gray-100 flex flex-col">
      {/* Header */}
      <header className="flex items-center justify-between px-4 py-2 bg-gray-800 border-b border-gray-700">
        <div className="flex items-center gap-2">
          <h1 className="text-base font-bold tracking-tight">
            OrderbookGC
          </h1>
          <span className="text-xs text-gray-500">Live Viewer</span>
        </div>
        <div className="flex items-center gap-2">
          <span
            className={`inline-block w-2 h-2 rounded-full ${
              connected ? 'bg-emerald-400' : 'bg-red-500 animate-pulse'
            }`}
          />
          <span className="text-xs text-gray-400">
            {connected ? 'Connected' : 'Disconnected'}
          </span>
        </div>
      </header>

      {/* Body */}
      <main className="flex-1 p-4 grid grid-cols-1 lg:grid-cols-3 gap-4">
        {/* Left — 2 cols on lg */}
        <div className="lg:col-span-2 space-y-4">
          <OrderbookView />
          <DepthChart />
          <PriceChart />
          <TradeHistory />
        </div>

        {/* Right — 1 col on lg */}
        <div className="space-y-4">
          <OrderEntry send={send} />
          <OrderList send={send} />
        </div>
      </main>

      {/* Toast layer */}
      <Notifications />
    </div>
  );
};

export default App;
