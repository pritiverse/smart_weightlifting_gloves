import React, { useState } from 'react';

interface HeaderProps {
  isConnected: boolean;
  latencyMs: number;
  onOpenCalibration: () => void;
  apiHost: string;
  onUpdateHost: (host: string) => void;
}

export const Header: React.FC<HeaderProps> = ({
  isConnected,
  latencyMs,
  onOpenCalibration,
  apiHost,
  onUpdateHost,
}) => {
  const [editingHost, setEditingHost] = useState(false);
  const [hostVal, setHostVal] = useState(apiHost);

  const handleHostSave = (e: React.FormEvent) => {
    e.preventDefault();
    onUpdateHost(hostVal);
    setEditingHost(false);
  };

  return (
    <header className="dashboard-header">
      <div className="brand-section">
        <div className="brand-icon">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#040814" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6" />
          </svg>
        </div>
        <div>
          <h1 className="brand-title">
            GRIP<span>GUARD</span>
          </h1>
          <p className="brand-subtitle">Real-Time Rep Validation & Sensor Reliability Intelligence</p>
        </div>
      </div>

      <div className="header-actions">
        {editingHost ? (
          <form onSubmit={handleHostSave} style={{ display: 'flex', gap: '6px' }}>
            <input
              type="text"
              value={hostVal}
              onChange={(e) => setHostVal(e.target.value)}
              style={{
                background: '#0a0f1d',
                border: '1px solid var(--border-highlight)',
                color: '#fff',
                padding: '6px 10px',
                borderRadius: '6px',
                fontSize: '12px',
                fontFamily: 'monospace',
              }}
            />
            <button type="submit" className="btn-secondary" style={{ padding: '6px 12px' }}>
              Save
            </button>
          </form>
        ) : (
          <button
            onClick={() => setEditingHost(true)}
            className="btn-secondary"
            title="Configure ESP32 IP Address"
          >
            <span style={{ color: 'var(--text-muted)' }}>Target:</span> {apiHost.replace('http://', '')}
          </button>
        )}

        <div className={`connection-badge ${isConnected ? 'connected' : 'disconnected'}`}>
          <div className="status-dot" />
          <span>{isConnected ? `ONLINE (${latencyMs}ms)` : 'OFFLINE / SEARCHING ESP32'}</span>
        </div>

        <button onClick={onOpenCalibration} className="btn-primary">
          <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
            <circle cx="12" cy="12" r="3" />
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" />
          </svg>
          Personal Calibration
        </button>
      </div>
    </header>
  );
};
