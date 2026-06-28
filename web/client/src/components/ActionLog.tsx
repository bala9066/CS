// Action log panel — user-facing audit trail. Mirrors the desktop action log dock.

import { useEffect, useRef } from 'react';
import type { LogEntry } from '../types';

export function ActionLog({ log }: { log: LogEntry[] }) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
  }, [log]);

  return (
    <div className="log" ref={ref}>
      {log.length === 0 ? (
        <div className="info">System initialized. Import a VDD to begin.</div>
      ) : (
        log.map((e, i) => (
          <div key={i} className={e.type}>
            <span className="muted">[{e.time}]</span> {e.msg}
          </div>
        ))
      )}
    </div>
  );
}
