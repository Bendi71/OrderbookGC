/**
 * PnL Dashboard — displays initial capital, realized/unrealized PnL,
 * equity, and open positions.
 */

import React, { useState, useCallback } from 'react';
import { useOrderbookStore, PnLState, PositionInfo } from '../store/orderbookStore';

interface Props {
  send: (data: Record<string, unknown>) => void;
}

const PnLDashboard: React.FC<Props> = ({ send }) => {
  const pnl = useOrderbookStore((s) => s.pnl);
  const [capitalInput, setCapitalInput] = useState('');
  const [settingCapital, setSettingCapital] = useState(false);

  const setCapital = useCallback(async () => {
    const amount = parseFloat(capitalInput);
    if (!amount || amount <= 0) return;
    setSettingCapital(true);
    try {
      const base = import.meta.env.DEV ? '' : '';
      await fetch(`${base}/api/pnl/capital`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amount }),
      });
      setCapitalInput('');
    } catch {
      // ignore
    } finally {
      setSettingCapital(false);
    }
  }, [capitalInput]);

  const pnlColor = (v: number) =>
    v > 0 ? 'text-emerald-400' : v < 0 ? 'text-red-400' : 'text-gray-400';

  const pnlSign = (v: number) => (v > 0 ? '+' : '');

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">PnL Dashboard</h2>
      </div>

      <div className="p-3 space-y-3">
        {/* Capital input */}
        <div className="flex gap-2">
          <input
            type="number"
            step="1000"
            min="1"
            value={capitalInput}
            onChange={(e) => setCapitalInput(e.target.value)}
            placeholder={`Capital: ${pnl.initial_capital.toLocaleString()}`}
            className="flex-1 bg-gray-900 border border-gray-700 rounded px-2 py-1 text-xs text-gray-200 focus:outline-none focus:ring-1 focus:ring-blue-500"
          />
          <button
            onClick={setCapital}
            disabled={settingCapital}
            className="bg-blue-600 hover:bg-blue-500 text-white text-xs font-semibold px-3 py-1 rounded disabled:opacity-50"
          >
            Set
          </button>
        </div>

        {/* Summary cards */}
        <div className="grid grid-cols-2 gap-2">
          <div className="bg-gray-900 rounded p-2">
            <div className="text-[10px] text-gray-500 uppercase">Equity</div>
            <div className={`text-sm font-bold ${pnlColor(pnl.equity - pnl.initial_capital)}`}>
              ${pnl.equity.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div className="bg-gray-900 rounded p-2">
            <div className="text-[10px] text-gray-500 uppercase">Total PnL</div>
            <div className={`text-sm font-bold ${pnlColor(pnl.total_pnl)}`}>
              {pnlSign(pnl.total_pnl)}${pnl.total_pnl.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div className="bg-gray-900 rounded p-2">
            <div className="text-[10px] text-gray-500 uppercase">Realized</div>
            <div className={`text-sm font-bold ${pnlColor(pnl.realized_pnl)}`}>
              {pnlSign(pnl.realized_pnl)}${pnl.realized_pnl.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div className="bg-gray-900 rounded p-2">
            <div className="text-[10px] text-gray-500 uppercase">Unrealized</div>
            <div className={`text-sm font-bold ${pnlColor(pnl.unrealized_pnl)}`}>
              {pnlSign(pnl.unrealized_pnl)}${pnl.unrealized_pnl.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
        </div>

        {/* Fills count */}
        <div className="text-[10px] text-gray-500">
          {pnl.fill_count} fill{pnl.fill_count !== 1 ? 's' : ''} recorded
        </div>

        {/* Positions table */}
        {pnl.positions.length > 0 && (
          <div className="overflow-x-auto">
            <table className="w-full text-xs">
              <thead className="text-[10px] text-gray-500 uppercase">
                <tr>
                  <th className="px-1 py-1 text-left">Symbol</th>
                  <th className="px-1 py-1 text-right">Qty</th>
                  <th className="px-1 py-1 text-right">Avg</th>
                  <th className="px-1 py-1 text-right">Last</th>
                  <th className="px-1 py-1 text-right">UPnL</th>
                  <th className="px-1 py-1 text-right">RPnL</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-700/50">
                {pnl.positions.map((pos) => (
                  <tr key={pos.symbol} className="hover:bg-gray-700/30">
                    <td className="px-1 py-1 font-semibold text-gray-200">{pos.symbol}</td>
                    <td className={`px-1 py-1 text-right font-semibold ${pos.quantity > 0 ? 'text-emerald-400' : pos.quantity < 0 ? 'text-red-400' : 'text-gray-400'}`}>
                      {pos.quantity}
                    </td>
                    <td className="px-1 py-1 text-right text-gray-300">{pos.avg_entry_price.toFixed(2)}</td>
                    <td className="px-1 py-1 text-right text-gray-300">{pos.last_price.toFixed(2)}</td>
                    <td className={`px-1 py-1 text-right ${pnlColor(pos.unrealized_pnl)}`}>
                      {pnlSign(pos.unrealized_pnl)}{pos.unrealized_pnl.toFixed(2)}
                    </td>
                    <td className={`px-1 py-1 text-right ${pnlColor(pos.realized_pnl)}`}>
                      {pnlSign(pos.realized_pnl)}{pos.realized_pnl.toFixed(2)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default PnLDashboard;
