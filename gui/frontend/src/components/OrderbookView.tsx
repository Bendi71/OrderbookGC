/**
 * Price-ladder orderbook view — bids (green) on left, asks (red) on right.
 * Each row shows price, total qty, and a shaded bar proportional to max qty.
 */

import React, { useMemo } from 'react';
import { useOrderbookStore, PriceLevel } from '../store/orderbookStore';

const ROWS = 15;

function formatPrice(p: number): string {
  return p.toFixed(2);
}
function formatQty(q: number): string {
  return q >= 1_000 ? `${(q / 1_000).toFixed(1)}K` : String(q);
}

const Row: React.FC<{
  level: PriceLevel | null;
  maxQty: number;
  side: 'bid' | 'ask';
}> = ({ level, maxQty, side }) => {
  const pct = level && maxQty > 0 ? (level.quantity / maxQty) * 100 : 0;
  const bg =
    side === 'bid'
      ? `linear-gradient(to left, rgba(34,197,94,0.25) ${pct}%, transparent ${pct}%)`
      : `linear-gradient(to right, rgba(239,68,68,0.25) ${pct}%, transparent ${pct}%)`;

  return (
    <div
      className="grid grid-cols-3 text-xs font-mono h-6 items-center px-2"
      style={{ background: bg }}
    >
      {side === 'bid' ? (
        <>
          <span className="text-right text-gray-400">
            {level ? level.order_count : ''}
          </span>
          <span className="text-right text-emerald-400">
            {level ? formatQty(level.quantity) : ''}
          </span>
          <span className="text-right font-semibold text-emerald-300">
            {level ? formatPrice(level.price) : ''}
          </span>
        </>
      ) : (
        <>
          <span className="text-left font-semibold text-red-300">
            {level ? formatPrice(level.price) : ''}
          </span>
          <span className="text-left text-red-400">
            {level ? formatQty(level.quantity) : ''}
          </span>
          <span className="text-left text-gray-400">
            {level ? level.order_count : ''}
          </span>
        </>
      )}
    </div>
  );
};

const OrderbookView: React.FC = () => {
  const bids = useOrderbookStore((s) => s.bids);
  const asks = useOrderbookStore((s) => s.asks);

  const best = useMemo(() => {
    const bestBid = bids.length > 0 ? bids[0].price : null;
    const bestAsk = asks.length > 0 ? asks[0].price : null;
    const spread =
      bestBid !== null && bestAsk !== null ? bestAsk - bestBid : null;
    return { bestBid, bestAsk, spread };
  }, [bids, asks]);

  const maxBidQty = useMemo(
    () => Math.max(...bids.slice(0, ROWS).map((l) => l.quantity), 1),
    [bids],
  );
  const maxAskQty = useMemo(
    () => Math.max(...asks.slice(0, ROWS).map((l) => l.quantity), 1),
    [asks],
  );

  // Pad arrays to ROWS
  const bidRows: (PriceLevel | null)[] = Array.from({ length: ROWS }, (_, i) =>
    i < bids.length ? bids[i] : null,
  );
  const askRows: (PriceLevel | null)[] = Array.from({ length: ROWS }, (_, i) =>
    i < asks.length ? asks[i] : null,
  );

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">Order Book</h2>
        {best.spread !== null && (
          <span className="text-xs text-gray-400">
            Spread: {best.spread.toFixed(2)}
          </span>
        )}
      </div>

      <div className="grid grid-cols-2 divide-x divide-gray-700">
        {/* Bids */}
        <div>
          <div className="grid grid-cols-3 text-[10px] text-gray-500 uppercase px-2 py-1 border-b border-gray-700">
            <span className="text-right">Cnt</span>
            <span className="text-right">Qty</span>
            <span className="text-right">Bid</span>
          </div>
          {bidRows.map((l, i) => (
            <Row key={i} level={l} maxQty={maxBidQty} side="bid" />
          ))}
        </div>

        {/* Asks */}
        <div>
          <div className="grid grid-cols-3 text-[10px] text-gray-500 uppercase px-2 py-1 border-b border-gray-700">
            <span className="text-left">Ask</span>
            <span className="text-left">Qty</span>
            <span className="text-left">Cnt</span>
          </div>
          {askRows.map((l, i) => (
            <Row key={i} level={l} maxQty={maxAskQty} side="ask" />
          ))}
        </div>
      </div>

      {/* Mid price */}
      {best.bestBid !== null && best.bestAsk !== null && (
        <div className="text-center text-xs text-yellow-400 py-1 border-t border-gray-700 font-semibold">
          Mid: {((best.bestBid + best.bestAsk) / 2).toFixed(2)}
        </div>
      )}
    </div>
  );
};

export default OrderbookView;
