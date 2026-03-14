/**
 * Login / Register form.
 * Renders a centered card over the dark background. The user can toggle
 * between "Log in" and "Register" modes.
 */

import React, { useState, useCallback } from 'react';
import { useOrderbookStore } from '../store/orderbookStore';

interface Props {
  send: (data: Record<string, unknown>) => void;
}

const LoginForm: React.FC<Props> = ({ send }) => {
  const [mode, setMode] = useState<'login' | 'register'>('login');
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [localError, setLocalError] = useState('');

  const authError = useOrderbookStore((s) => s.authError);
  const connected = useOrderbookStore((s) => s.connected);

  const handleSubmit = useCallback(
    (e: React.FormEvent) => {
      e.preventDefault();
      setLocalError('');

      if (!username.trim()) {
        setLocalError('Username is required');
        return;
      }

      if (mode === 'register') {
        if (!password) {
          setLocalError('Password is required');
          return;
        }
        if (password !== confirmPassword) {
          setLocalError('Passwords do not match');
          return;
        }
        send({ action: 'register', username: username.trim(), password });
      } else {
        send({ action: 'login', username: username.trim(), password });
      }
    },
    [mode, username, password, confirmPassword, send],
  );

  const toggleMode = () => {
    setMode((m) => (m === 'login' ? 'register' : 'login'));
    setLocalError('');
  };

  const error = localError || authError;

  return (
    <div className="min-h-screen bg-gray-900 flex items-center justify-center p-4">
      <div className="w-full max-w-sm bg-gray-800 rounded-lg shadow-xl border border-gray-700 p-6">
        {/* Logo / Title */}
        <div className="text-center mb-6">
          <h1 className="text-2xl font-bold text-gray-100 tracking-tight">
            OrderbookGC
          </h1>
          <p className="text-sm text-gray-400 mt-1">
            {mode === 'login' ? 'Sign in to your account' : 'Create a new account'}
          </p>
        </div>

        {/* Connection indicator */}
        <div className="flex items-center justify-center gap-2 mb-4">
          <span
            className={`inline-block w-2 h-2 rounded-full ${
              connected ? 'bg-emerald-400' : 'bg-red-500 animate-pulse'
            }`}
          />
          <span className="text-xs text-gray-400">
            {connected ? 'Server connected' : 'Server disconnected'}
          </span>
        </div>

        {/* Error */}
        {error && (
          <div className="bg-red-900/40 border border-red-700 text-red-300 text-sm rounded px-3 py-2 mb-4">
            {error}
          </div>
        )}

        <form onSubmit={handleSubmit} className="space-y-4">
          {/* Username */}
          <div>
            <label className="block text-xs font-medium text-gray-400 mb-1">
              Username
            </label>
            <input
              type="text"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              className="w-full bg-gray-700 border border-gray-600 rounded px-3 py-2 text-sm text-gray-100 placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              placeholder="Enter username"
              autoFocus
            />
          </div>

          {/* Password */}
          <div>
            <label className="block text-xs font-medium text-gray-400 mb-1">
              Password
            </label>
            <input
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              className="w-full bg-gray-700 border border-gray-600 rounded px-3 py-2 text-sm text-gray-100 placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              placeholder="Enter password"
            />
          </div>

          {/* Confirm password (register only) */}
          {mode === 'register' && (
            <div>
              <label className="block text-xs font-medium text-gray-400 mb-1">
                Confirm Password
              </label>
              <input
                type="password"
                value={confirmPassword}
                onChange={(e) => setConfirmPassword(e.target.value)}
                className="w-full bg-gray-700 border border-gray-600 rounded px-3 py-2 text-sm text-gray-100 placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                placeholder="Re-enter password"
              />
            </div>
          )}

          {/* Submit */}
          <button
            type="submit"
            disabled={!connected}
            className="w-full bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white font-medium text-sm rounded px-4 py-2 transition-colors"
          >
            {mode === 'login' ? 'Log in' : 'Register'}
          </button>
        </form>

        {/* Toggle */}
        <p className="text-center text-xs text-gray-400 mt-4">
          {mode === 'login' ? (
            <>
              Don&apos;t have an account?{' '}
              <button
                onClick={toggleMode}
                className="text-blue-400 hover:text-blue-300 underline"
              >
                Register
              </button>
            </>
          ) : (
            <>
              Already have an account?{' '}
              <button
                onClick={toggleMode}
                className="text-blue-400 hover:text-blue-300 underline"
              >
                Log in
              </button>
            </>
          )}
        </p>
      </div>
    </div>
  );
};

export default LoginForm;
