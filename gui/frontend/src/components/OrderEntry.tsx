/**
 * Order entry form — buy / sell toggle, price, quantity, submit.
 */

import React, { useState, useCallback } from 'react';

interface Props {
  send: (data: Record<string, unknown>) => void;
  symbols?: string[];
}

const OrderEntry: React.FC<Props> = ({ send, symbols = ['AAPL'] }) => {
  const [side, setSide] = useState<'BUY' | 'SELL'>('BUY');
  const [orderType, setOrderType] = useState<'LIMIT' | 'MARKET'>('LIMIT');
  const [symbol, setSymbol] = useState(symbols[0] ?? 'AAPL');
  const [price, setPrice] = useState('');
  const [quantity, setQuantity] = useState('');

  const submit = useCallback(
    (e: React.FormEvent) => {
      e.preventDefault();
      const q = parseInt(quantity, 10);
      if (!q || q <= 0) return;

      if (orderType === 'LIMIT') {
        const p = parseFloat(price);
        if (!p || p <= 0) return;
        send({
          action: 'submit_order',
          symbol,
          side,
          order_type: 'LIMIT',
          price: p,
          quantity: q,
        });
      } else {
        send({
          action: 'submit_order',
          symbol,
          side,
          order_type: 'MARKET',
          price: 0,
          quantity: q,
        });
      }
      setPrice('');
      setQuantity('');
    },
    [send, symbol, side, orderType, price, quantity],
  );

  const isBuy = side === 'BUY';
  const isMarket = orderType === 'MARKET';

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">New Order</h2>
      </div>

      <form onSubmit={submit} className="p-3 space-y-3">
        {/* Side toggle */}
        <div className="grid grid-cols-2 gap-1 bg-gray-900 rounded p-0.5">
          <button
            type="button"
            onClick={() => setSide('BUY')}
            className={`text-xs font-bold py-1.5 rounded transition-colors ${
              isBuy
                ? 'bg-emerald-600 text-white'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            BUY
          </button>
          <button
            type="button"
            onClick={() => setSide('SELL')}
            className={`text-xs font-bold py-1.5 rounded transition-colors ${
              !isBuy
                ? 'bg-red-600 text-white'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            SELL
          </button>
        </div>

        {/* Order Type toggle */}
        <div className="grid grid-cols-2 gap-1 bg-gray-900 rounded p-0.5">
          <button
            type="button"
            onClick={() => setOrderType('LIMIT')}
            className={`text-xs font-bold py-1.5 rounded transition-colors ${
              !isMarket
                ? 'bg-blue-600 text-white'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            LIMIT
          </button>
          <button
            type="button"
            onClick={() => setOrderType('MARKET')}
            className={`text-xs font-bold py-1.5 rounded transition-colors ${
              isMarket
                ? 'bg-amber-600 text-white'
                : 'text-gray-400 hover:text-gray-200'
            }`}
          >
            MARKET
          </button>
        </div>

        {/* Symbol */}
        <div>
          <label className="text-[10px] text-gray-500 uppercase">Symbol</label>
          <select
            value={symbol}
            onChange={(e) => setSymbol(e.target.value)}
            className="w-full bg-gray-900 border border-gray-700 rounded px-2 py-1.5 text-sm text-gray-200 focus:outline-none focus:ring-1 focus:ring-blue-500"
          >
            {symbols.map((s) => (
              <option key={s} value={s}>
                {s}
              </option>
            ))}
          </select>
        </div>

        {/* Price (limit only) */}
        {!isMarket && (
        <div>
          <label className="text-[10px] text-gray-500 uppercase">Price</label>
          <input
            type="number"
            step="0.01"
            min="0.01"
            value={price}
            onChange={(e) => setPrice(e.target.value)}
            placeholder="0.00"
            className="w-full bg-gray-900 border border-gray-700 rounded px-2 py-1.5 text-sm text-gray-200 focus:outline-none focus:ring-1 focus:ring-blue-500"
          />
        </div>
        )}

        {/* Quantity */}
        <div>
          <label className="text-[10px] text-gray-500 uppercase">
            Quantity
          </label>
          <input
            type="number"
            step="1"
            min="1"
            value={quantity}
            onChange={(e) => setQuantity(e.target.value)}
            placeholder="0"
            className="w-full bg-gray-900 border border-gray-700 rounded px-2 py-1.5 text-sm text-gray-200 focus:outline-none focus:ring-1 focus:ring-blue-500"
          />
        </div>

        <button
          type="submit"
          className={`w-full py-2 rounded text-sm font-bold transition-colors ${
            isBuy
              ? 'bg-emerald-600 hover:bg-emerald-500 text-white'
              : 'bg-red-600 hover:bg-red-500 text-white'
          }`}
        >
          {isBuy
            ? isMarket ? 'Market Buy' : 'Place Buy Order'
            : isMarket ? 'Market Sell' : 'Place Sell Order'}
        </button>
      </form>
    </div>
  );
};

export default OrderEntry;
