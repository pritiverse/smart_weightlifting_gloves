import React, { useState } from 'react';
import { HealthData } from '../types';

const HOME_IP  = 'http://10.249.34.96';
const GLOVE_AP = 'http://192.168.4.1';
const SIM_IP   = 'http://localhost:8000';

interface HeaderProps {
  isConnected: boolean;
  latencyMs: number;
  onOpenCalibration: () => void;
  apiHost: string;
  onUpdateHost: (host: string) => void;
  health?: HealthData;
}

export const Header: React.FC<HeaderProps> = ({
  isConnected,
  latencyMs,
  onOpenCalibration,
  apiHost,
  onUpdateHost,
  health,
}) => {
  const [showCustom, setShowCustom] = useState(false);
  const [customVal, setCustomVal]   = useState(apiHost);

  const isHome  = apiHost === HOME_IP;
  const isGlove = apiHost === GLOVE_AP;
  const isSim   = apiHost === SIM_IP || apiHost.includes('localhost:8000') || apiHost.includes('127.0.0.1:8000');

  const handleSwitch = (target: 'home' | 'glove' | 'sim') => {
    onUpdateHost(target === 'home' ? HOME_IP : target === 'glove' ? GLOVE_AP : SIM_IP);
    setShowCustom(false);
  };

  const handleCustomSave = (e: React.FormEvent) => {
    e.preventDefault();
    onUpdateHost(customVal);
    setShowCustom(false);
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
          <h1 className="brand-title">GRIP<span>GUARD</span></h1>
          <p className="brand-subtitle">Real-Time Rep Validation &amp; Sensor Reliability Intelligence</p>
        </div>
      </div>

      <div className="header-actions">

        {/* ── WiFi Source Toggle ── */}
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '4px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase' }}>
              Network:
            </span>
            <div style={{
              display: 'flex',
              background: 'var(--bg-surface)',
              border: '1px solid var(--border-subtle)',
              borderRadius: '20px',
              padding: '3px',
              gap: '2px',
            }}>
              <button
                onClick={() => handleSwitch('home')}
                title={`Home WiFi — ${HOME_IP}`}
                style={{
                  padding: '5px 13px',
                  borderRadius: '16px',
                  fontSize: '12px',
                  fontWeight: 700,
                  transition: 'all 0.2s',
                  background: isHome ? 'var(--cyan-primary)' : 'transparent',
                  color: isHome ? '#040814' : 'var(--text-muted)',
                  border: 'none',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '5px',
                }}
              >
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                  <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>
                  <polyline points="9 22 9 12 15 12 15 22"/>
                </svg>
                Home WiFi
              </button>
              <button
                onClick={() => handleSwitch('glove')}
                title={`Glove Hotspot — ${GLOVE_AP}`}
                style={{
                  padding: '5px 13px',
                  borderRadius: '16px',
                  fontSize: '12px',
                  fontWeight: 700,
                  transition: 'all 0.2s',
                  background: isGlove ? 'rgba(180,100,255,0.85)' : 'transparent',
                  color: isGlove ? '#fff' : 'var(--text-muted)',
                  border: 'none',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '5px',
                }}
              >
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                  <path d="M5 12.55a11 11 0 0 1 14.08 0"/>
                  <path d="M1.42 9a16 16 0 0 1 21.16 0"/>
                  <path d="M8.53 16.11a6 6 0 0 1 6.95 0"/>
                  <circle cx="12" cy="20" r="1" fill="currentColor"/>
                </svg>
                Glove AP
              </button>
              <button
                onClick={() => handleSwitch('sim')}
                title={`Simulation Server — ${SIM_IP}`}
                style={{
                  padding: '5px 13px',
                  borderRadius: '16px',
                  fontSize: '12px',
                  fontWeight: 700,
                  transition: 'all 0.2s',
                  background: isSim ? 'linear-gradient(135deg, #00f2fe, #4facfe)' : 'transparent',
                  color: isSim ? '#040814' : 'var(--text-muted)',
                  border: 'none',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '5px',
                }}
              >
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                  <polygon points="5 3 19 12 5 21 5 3"/>
                </svg>
                Simulator
              </button>
            </div>

            {/* Custom IP override */}
            <button
              onClick={() => setShowCustom((v) => !v)}
              title="Set custom IP"
              style={{
                background: 'var(--bg-surface)',
                border: '1px solid var(--border-subtle)',
                borderRadius: '6px',
                padding: '5px 7px',
                cursor: 'pointer',
                color: 'var(--text-muted)',
                lineHeight: 1,
              }}
            >
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                <circle cx="12" cy="12" r="3"/>
                <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/>
              </svg>
            </button>
          </div>

          {/* Custom IP input — shown only when gear clicked */}
          {showCustom && (
            <form onSubmit={handleCustomSave} style={{ display: 'flex', gap: '6px' }}>
              <input
                type="text"
                value={customVal}
                onChange={(e) => setCustomVal(e.target.value)}
                placeholder="http://192.168.x.x"
                style={{
                  background: '#0a0f1d',
                  border: '1px solid var(--border-highlight)',
                  color: '#fff',
                  padding: '5px 10px',
                  borderRadius: '6px',
                  fontSize: '12px',
                  fontFamily: 'monospace',
                  width: '180px',
                }}
              />
              <button type="submit" className="btn-secondary" style={{ padding: '5px 10px', fontSize: '12px' }}>
                Apply
              </button>
            </form>
          )}
        </div>

        {/* ── Live Sensor Hardware Status Indicator ── */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          background: 'var(--bg-surface)',
          border: '1px solid var(--border-subtle)',
          borderRadius: '20px',
          padding: '6px 14px',
          fontSize: '11px',
          fontWeight: 600,
        }}>
          <span style={{ color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.04em' }}>Sensors:</span>
          <span style={{
            display: 'flex', alignItems: 'center', gap: '5px',
            color: isConnected && health?.mpu === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)'
          }}>
            <span style={{
              width: '7px', height: '7px', borderRadius: '50%',
              background: isConnected && health?.mpu === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)',
              display: 'inline-block'
            }} />
            IMU {isConnected ? (health?.mpu === 'HEALTHY' ? 'Online' : health?.mpu || 'Degraded') : 'Offline'}
          </span>
          <span style={{ color: 'var(--border-subtle)' }}>|</span>
          <span style={{
            display: 'flex', alignItems: 'center', gap: '5px',
            color: isConnected && health?.fsr === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)'
          }}>
            <span style={{
              width: '7px', height: '7px', borderRadius: '50%',
              background: isConnected && health?.fsr === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)',
              display: 'inline-block'
            }} />
            FSR {isConnected ? (health?.fsr === 'HEALTHY' ? 'Online' : health?.fsr || 'Degraded') : 'Offline'}
          </span>
        </div>

        {/* ── Connection Status ── */}
        <div className={`connection-badge ${isConnected ? 'connected' : 'disconnected'}`}>
          <div className="status-dot" />
          <span>{isConnected ? `ONLINE · ${latencyMs}ms` : 'OFFLINE / SEARCHING'}</span>
        </div>

        {/* ── Calibration ── */}
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
