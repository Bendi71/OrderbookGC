/**
 * Scrolling recent trades list.
 */

import React from 'react';
import { useOrderbookStore } from '../store/orderbookStore';

const TradeHistory: React.FC = () => {
  const trades = useOrderbookStore((s) => s.trades);

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">Recent Trades</h2>
      </div>

      {trades.length === 0 ? (
        <div className="p-4 text-center text-gray-500 text-xs">
          No trades yet
        </div>
      ) : (
        <div className="overflow-y-auto max-h-60">
          <table className="w-full text-xs">
            <thead className="text-[10px] text-gray-500 uppercase sticky top-0 bg-gray-800">
              <tr>
                <th className="px-2 py-1 text-right">Price</th>
                <th className="px-2 py-1 text-right">Qty</th>
                <th className="px-2 py-1 text-left">Symbol</th>
                <th className="px-2 py-1 text-left">Time</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-gray-700/50">
              {trades.map((t, i) => {
                const time = t.timestamp
                  ? new Date(t.timestamp).toLocaleTimeString()
                  : '';

                return (
                  <tr key={`${t.buy_order_id}-${i}`} className="hover:bg-gray-700/30">
                    <td className="px-2 py-1 text-right font-mono text-yellow-300">
                      {t.price.toFixed(2)}
                    </td>
                    <td className="px-2 py-1 text-right text-gray-300">
                      {t.quantity}
                    </td>
                    <td className="px-2 py-1 text-gray-400">{t.symbol}</td>
                    <td className="px-2 py-1 text-gray-500">{time}</td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
};

export default TradeHistory;
