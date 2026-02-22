/**
 * Historical price candlestick chart.
 * Aggregates trade prices into OHLC candles and renders them
 * using Recharts with a custom candlestick shape (body + wicks).
 */

import React, { useMemo } from 'react';
import {
  ComposedChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  Cell,
} from 'recharts';
import { useOrderbookStore, PricePoint } from '../store/orderbookStore';

/* ─── types ────────────────────────────────────────────────── */

interface Candle {
  time: number;
  timeLabel: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  range: [number, number]; // [low, high] for Recharts range bar
}

/* ─── constants ────────────────────────────────────────────── */

const CANDLE_INTERVAL_MS = 3000; // 3-second candles
const MAX_VISIBLE_CANDLES = 60;

/* ─── helpers ──────────────────────────────────────────────── */

function formatTime(ts: number): string {
  const d = new Date(ts);
  const hh = d.getHours().toString().padStart(2, '0');
  const mm = d.getMinutes().toString().padStart(2, '0');
  const ss = d.getSeconds().toString().padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

function buildCandles(history: PricePoint[]): Candle[] {
  if (!history.length) return [];

  const buckets = new Map<number, PricePoint[]>();

  for (const p of history) {
    const bucket = Math.floor(p.time / CANDLE_INTERVAL_MS) * CANDLE_INTERVAL_MS;
    let arr = buckets.get(bucket);
    if (!arr) {
      arr = [];
      buckets.set(bucket, arr);
    }
    arr.push(p);
  }

  const candles: Candle[] = [];

  for (const [time, points] of Array.from(buckets.entries()).sort(
    (a, b) => a[0] - b[0],
  )) {
    const prices = points.map((p) => p.price);
    const o = prices[0];
    const c = prices[prices.length - 1];
    const h = Math.max(...prices);
    const l = Math.min(...prices);

    candles.push({
      time,
      timeLabel: formatTime(time),
      open: o,
      high: h,
      low: l,
      close: c,
      volume: points.reduce((s, p) => s + p.quantity, 0),
      range: [l, h],
    });
  }

  // Keep only the most recent candles for readability
  return candles.slice(-MAX_VISIBLE_CANDLES);
}

/* ─── custom candlestick bar shape ─────────────────────────── */

/**
 * Draws a single candlestick: body (rect) + upper/lower wicks (lines).
 *
 * When dataKey="range" returns [low, high], Recharts positions the bar so
 * that `y` = pixel y of `high` and `y + height` = pixel y of `low`.
 * We derive body and wick positions from that mapping.
 */
const CandlestickShape = (props: unknown) => {
  const { x, y, width, height, payload } = props as {
    x: number;
    y: number;
    width: number;
    height: number;
    payload: Candle;
  };

  if (!payload || height == null) return <g />;

  const { open, close, high, low } = payload;
  const dataRange = high - low;

  // Doji — high === low → flat horizontal line
  if (dataRange === 0) {
    const cy = y + (height ?? 0) / 2;
    return (
      <line
        x1={x}
        y1={cy}
        x2={x + width}
        y2={cy}
        stroke="#9ca3af"
        strokeWidth={2}
      />
    );
  }

  const isUp = close >= open;
  const fill = isUp ? '#10b981' : '#ef4444';
  const pxPerUnit = height / dataRange;

  const bodyTop = Math.max(open, close);
  const bodyBottom = Math.min(open, close);
  const bodyY = y + (high - bodyTop) * pxPerUnit;
  const bodyH = Math.max((bodyTop - bodyBottom) * pxPerUnit, 1);

  const cx = x + width / 2;
  const bodyPad = width * 0.15; // slight inset for aesthetics

  return (
    <g>
      {/* upper wick */}
      <line x1={cx} y1={y} x2={cx} y2={bodyY} stroke={fill} strokeWidth={1} />
      {/* body */}
      <rect
        x={x + bodyPad}
        y={bodyY}
        width={width - bodyPad * 2}
        height={bodyH}
        fill={fill}
      />
      {/* lower wick */}
      <line
        x1={cx}
        y1={bodyY + bodyH}
        x2={cx}
        y2={y + height}
        stroke={fill}
        strokeWidth={1}
      />
    </g>
  );
};

/* ─── custom tooltip ───────────────────────────────────────── */

const CandleTooltip: React.FC<{ active?: boolean; payload?: any[] }> = ({
  active,
  payload,
}) => {
  if (!active || !payload?.[0]) return null;
  const c = payload[0].payload as Candle;
  const col = c.close >= c.open ? 'text-emerald-400' : 'text-red-400';

  return (
    <div className="bg-gray-900 border border-gray-700 rounded px-3 py-2 text-xs space-y-0.5 shadow-lg">
      <div className="text-gray-400 font-medium">{c.timeLabel}</div>
      <div>
        O: <span className="text-gray-200">{c.open.toFixed(2)}</span>
      </div>
      <div>
        H: <span className="text-gray-200">{c.high.toFixed(2)}</span>
      </div>
      <div>
        L: <span className="text-gray-200">{c.low.toFixed(2)}</span>
      </div>
      <div>
        C: <span className={col}>{c.close.toFixed(2)}</span>
      </div>
      <div>
        Vol: <span className="text-gray-200">{c.volume}</span>
      </div>
    </div>
  );
};

/* ─── main component ───────────────────────────────────────── */

const PriceChart: React.FC = () => {
  const priceHistory = useOrderbookStore((s) => s.priceHistory);

  const candles = useMemo(() => buildCandles(priceHistory), [priceHistory]);

  // Y domain with padding so wicks aren't clipped
  const yDomain = useMemo<[number, number]>(() => {
    if (!candles.length) return [0, 1];
    const lo = Math.min(...candles.map((c) => c.low));
    const hi = Math.max(...candles.map((c) => c.high));
    const pad = (hi - lo) * 0.08 || 0.5;
    return [lo - pad, hi + pad];
  }, [candles]);

  if (!candles.length) {
    return (
      <div className="bg-gray-800 rounded-lg p-4 text-center text-gray-500 text-sm">
        Waiting for trades to build price history…
      </div>
    );
  }

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-xs font-semibold text-gray-400 mb-2 uppercase tracking-wide">
        Price Chart{' '}
        <span className="text-gray-600 font-normal">
          ({CANDLE_INTERVAL_MS / 1000}s candles)
        </span>
      </h2>

      <ResponsiveContainer width="100%" height={280}>
        <ComposedChart
          data={candles}
          margin={{ top: 8, right: 16, bottom: 0, left: 8 }}
        >
          <XAxis
            dataKey="timeLabel"
            tick={{ fill: '#9ca3af', fontSize: 10 }}
            axisLine={{ stroke: '#374151' }}
            tickLine={false}
            interval="preserveStartEnd"
            minTickGap={40}
          />
          <YAxis
            domain={yDomain}
            tick={{ fill: '#9ca3af', fontSize: 10 }}
            axisLine={false}
            tickLine={false}
            width={54}
            tickFormatter={(v: number) => v.toFixed(2)}
          />
          <Tooltip content={<CandleTooltip />} />
          <Bar dataKey="range" shape={CandlestickShape} barSize={10}>
            {candles.map((c, i) => (
              <Cell
                key={i}
                fill={c.close >= c.open ? '#10b981' : '#ef4444'}
              />
            ))}
          </Bar>
        </ComposedChart>
      </ResponsiveContainer>
    </div>
  );
};

export default PriceChart;
