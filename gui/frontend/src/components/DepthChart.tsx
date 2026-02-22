/**
 * Cumulative depth chart — green area for bids, red area for asks.
 * Uses Recharts AreaChart.
 */

import React, { useMemo } from 'react';
import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
} from 'recharts';
import { useOrderbookStore } from '../store/orderbookStore';

interface DepthPoint {
  price: number;
  bidQty: number | null;
  askQty: number | null;
}

const DepthChart: React.FC = () => {
  const bids = useOrderbookStore((s) => s.bids);
  const asks = useOrderbookStore((s) => s.asks);

  const { data, midPrice } = useMemo(() => {
    // Build cumulative bid curve (highest → lowest price)
    const bidPoints: DepthPoint[] = [];
    let cumBid = 0;
    for (let i = 0; i < bids.length; i++) {
      cumBid += bids[i].quantity;
      bidPoints.push({ price: bids[i].price, bidQty: cumBid, askQty: null });
    }
    bidPoints.reverse(); // lowest → highest for chart x-axis

    // Build cumulative ask curve (lowest → highest price)
    const askPoints: DepthPoint[] = [];
    let cumAsk = 0;
    for (let i = 0; i < asks.length; i++) {
      cumAsk += asks[i].quantity;
      askPoints.push({ price: asks[i].price, bidQty: null, askQty: cumAsk });
    }

    const bestBid = bids.length > 0 ? bids[0].price : 0;
    const bestAsk = asks.length > 0 ? asks[0].price : 0;
    const mid = bestBid && bestAsk ? (bestBid + bestAsk) / 2 : 0;

    return { data: [...bidPoints, ...askPoints], midPrice: mid };
  }, [bids, asks]);

  if (data.length === 0) {
    return (
      <div className="bg-gray-800 rounded-lg border border-gray-700 p-4 flex items-center justify-center text-gray-500 text-sm h-48">
        No depth data
      </div>
    );
  }

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="px-3 py-2 border-b border-gray-700">
        <h2 className="text-sm font-semibold text-gray-200">Market Depth</h2>
      </div>
      <ResponsiveContainer width="100%" height={200}>
        <AreaChart data={data} margin={{ top: 8, right: 16, bottom: 4, left: 0 }}>
          <XAxis
            dataKey="price"
            type="number"
            domain={['dataMin', 'dataMax']}
            tickFormatter={(v: number) => v.toFixed(0)}
            tick={{ fontSize: 10, fill: '#9ca3af' }}
            axisLine={{ stroke: '#374151' }}
            tickLine={false}
          />
          <YAxis
            tick={{ fontSize: 10, fill: '#9ca3af' }}
            axisLine={false}
            tickLine={false}
            width={40}
          />
          <Tooltip
            contentStyle={{
              background: '#1f2937',
              border: '1px solid #374151',
              borderRadius: 6,
              fontSize: 12,
            }}
            formatter={(value: number) => value.toLocaleString()}
            labelFormatter={(v: number) => `Price: ${Number(v).toFixed(2)}`}
          />
          {midPrice > 0 && (
            <ReferenceLine
              x={midPrice}
              stroke="#facc15"
              strokeDasharray="4 4"
              strokeWidth={1}
            />
          )}
          <Area
            type="stepAfter"
            dataKey="bidQty"
            stroke="#22c55e"
            fill="#22c55e"
            fillOpacity={0.2}
            connectNulls={false}
            name="Bids"
            dot={false}
          />
          <Area
            type="stepAfter"
            dataKey="askQty"
            stroke="#ef4444"
            fill="#ef4444"
            fillOpacity={0.2}
            connectNulls={false}
            name="Asks"
            dot={false}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
};

export default DepthChart;
