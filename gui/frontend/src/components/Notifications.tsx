/**
 * Toast notifications via react-hot-toast for fills, cancels, connection events.
 */

import React, { useEffect, useRef } from 'react';
import toast, { Toaster } from 'react-hot-toast';
import { useOrderbookStore } from '../store/orderbookStore';

const Notifications: React.FC = () => {
  const notifications = useOrderbookStore((s) => s.notifications);
  const removeNotification = useOrderbookStore((s) => s.removeNotification);
  const shownRef = useRef(new Set<string>());

  useEffect(() => {
    for (const n of notifications) {
      if (shownRef.current.has(n.id)) continue;
      shownRef.current.add(n.id);

      switch (n.type) {
        case 'success':
          toast.success(n.message, { duration: 3000 });
          break;
        case 'error':
          toast.error(n.message, { duration: 4000 });
          break;
        default:
          toast(n.message, { duration: 2500 });
      }

      // Remove from store after showing
      setTimeout(() => removeNotification(n.id), 5000);
    }
  }, [notifications, removeNotification]);

  // Clean up shown set periodically to prevent memory leak
  useEffect(() => {
    const iv = setInterval(() => {
      if (shownRef.current.size > 200) {
        shownRef.current.clear();
      }
    }, 60_000);
    return () => clearInterval(iv);
  }, []);

  return (
    <Toaster
      position="bottom-right"
      toastOptions={{
        style: {
          background: '#1f2937',
          color: '#e5e7eb',
          border: '1px solid #374151',
          fontSize: 12,
        },
      }}
    />
  );
};

export default Notifications;
