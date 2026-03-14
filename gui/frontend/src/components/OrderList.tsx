/**
 * Active / recent orders table with cancel buttons.
 */

import React, { useCallback } from 'react';
import { useOrderbookStore, OrderInfo } from '../store/orderbookStore';

interface Props {
  send: (data: Record<string, unknown>) => void;
}

const statusColor: Record<string, string> = {
  PENDING: 'text-yellow-400',
  ACCEPTED: 'text-blue-400',
  PARTIAL: 'text-cyan-400',
  FILLED: 'text-emerald-400',
  CANCELED: 'text-gray-500',
};

const OrderList: React.FC<Props> = ({ send }) => {
  const orders = useOrderbookStore((s) => s.orders);

  const cancelOrder = useCallback(
    (id: string) => {
      send({ action: 'cancel_order', order_id: id });
    },
    [send],
  );

  const orderArr = Array.from(orders.values()).sort(
    (a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime(),
  );

  const canCancel = (o: OrderInfo) =>
    o.status !== 'FILLED' && o.status !== 'CANCELED';

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">Orders</h2>
      </div>

      {orderArr.length === 0 ? (
        <div className="p-4 text-center text-gray-500 text-xs">
          No orders yet
        </div>
      ) : (
        <div className="overflow-x-auto max-h-60 overflow-y-auto">
          <table className="w-full text-xs">
            <thead className="text-[10px] text-gray-500 uppercase sticky top-0 bg-gray-800">
              <tr>
                <th className="px-2 py-1 text-left">ID</th>
                <th className="px-2 py-1 text-left">Type</th>
                <th className="px-2 py-1 text-left">Side</th>
                <th className="px-2 py-1 text-right">Price</th>
                <th className="px-2 py-1 text-right">Stop</th>
                <th className="px-2 py-1 text-right">Qty</th>
                <th className="px-2 py-1 text-right">Filled</th>
                <th className="px-2 py-1 text-left">Status</th>
                <th className="px-2 py-1" />
              </tr>
            </thead>
            <tbody className="divide-y divide-gray-700/50">
              {orderArr.map((o) => (
                <tr key={o.order_id} className="hover:bg-gray-700/30">
                  <td className="px-2 py-1 font-mono text-gray-400">
                    {o.order_id.slice(0, 8)}
                  </td>
                  <td className={`px-2 py-1 text-[10px] font-semibold ${
                    o.order_type === 'MARKET' ? 'text-amber-400'
                    : o.order_type === 'STOP' ? 'text-purple-400'
                    : o.order_type === 'STOP_LIMIT' ? 'text-indigo-400'
                    : 'text-blue-400'
                  }`}>
                    {o.order_type === 'STOP_LIMIT' ? 'S-LMT' : o.order_type ?? 'LMT'}
                  </td>
                  <td
                    className={`px-2 py-1 font-semibold ${
                      o.side === 'BUY' ? 'text-emerald-400' : 'text-red-400'
                    }`}
                  >
                    {o.side}
                  </td>
                  <td className="px-2 py-1 text-right text-gray-300">
                    {o.order_type === 'MARKET' || o.order_type === 'STOP' ? 'MKT' : o.price.toFixed(2)}
                  </td>
                  <td className="px-2 py-1 text-right text-gray-400">
                    {(o.order_type === 'STOP' || o.order_type === 'STOP_LIMIT') && o.stop_price > 0
                      ? o.stop_price.toFixed(2)
                      : '—'}
                  </td>
                  <td className="px-2 py-1 text-right text-gray-300">
                    {o.quantity}
                  </td>
                  <td className="px-2 py-1 text-right text-gray-300">
                    {o.filled_quantity}
                  </td>
                  <td
                    className={`px-2 py-1 ${
                      statusColor[o.status] ?? 'text-gray-400'
                    }`}
                  >
                    {o.status}
                  </td>
                  <td className="px-2 py-1 text-right">
                    {canCancel(o) && (
                      <button
                        onClick={() => cancelOrder(o.order_id)}
                        className="text-red-500 hover:text-red-300 text-[10px] font-semibold"
                      >
                        ✕
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
};

export default OrderList;
