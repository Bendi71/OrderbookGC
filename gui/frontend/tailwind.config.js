/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        bid: { DEFAULT: '#16a34a', light: '#22c55e', bg: '#052e16' },
        ask: { DEFAULT: '#dc2626', light: '#ef4444', bg: '#450a0a' },
      },
    },
  },
  plugins: [],
};
