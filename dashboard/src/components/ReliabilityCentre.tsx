import React from 'react';
import { HealthData } from '../types';

interface ReliabilityCentreProps {
  health: HealthData;
}

export const ReliabilityCentre: React.FC<ReliabilityCentreProps> = ({ health }) => {
  return (
    <div className="dashboard-card grid-col-6">
      <div className="card-header">
        <h2 className="card-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--cyan-primary)" strokeWidth="2.5">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z" />
          </svg>
          Sensor Reliability Centre
        </h2>
        <div className={`health-badge ${health.overall}`}>
          {health.overall}
        </div>
      </div>

      {/* Grid of Diagnostic Health Indicators */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '14px' }}>
        <div style={{ background: 'var(--bg-surface)', padding: '14px', borderRadius: 'var(--radius-md)' }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase' }}>
            MPU6050 6-Axis IMU
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: '6px' }}>
            <span style={{ fontSize: '13px', fontWeight: 700, color: health.mpu === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)' }}>
              {health.mpu}
            </span>
            <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>I2C @ 0x68</span>
          </div>
        </div>

        <div style={{ background: 'var(--bg-surface)', padding: '14px', borderRadius: 'var(--radius-md)' }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase' }}>
            FSR Analog Circuit
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: '6px' }}>
            <span style={{ fontSize: '13px', fontWeight: 700, color: health.fsr === 'HEALTHY' ? 'var(--emerald-valid)' : 'var(--crimson-fault)' }}>
              {health.fsr}
            </span>
            <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>GPIO 34 ADC1</span>
          </div>
        </div>
      </div>

      {/* Confidence Score Bar */}
      <div style={{ marginTop: '16px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
          <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>Diagnostic Assessment Confidence</span>
          <span style={{ color: 'var(--cyan-primary)', fontFamily: 'var(--font-mono)', fontWeight: 700 }}>
            {health.confidence_pct}%
          </span>
        </div>
        <div style={{ width: '100%', height: '8px', background: '#090e18', borderRadius: '4px', overflow: 'hidden' }}>
          <div
            style={{
              height: '100%',
              width: `${health.confidence_pct}%`,
              background: health.confidence_pct >= 80 ? 'var(--emerald-valid)' : health.confidence_pct >= 50 ? 'var(--amber-warning)' : 'var(--crimson-fault)',
              transition: 'width 0.3s ease',
            }}
          />
        </div>
      </div>

      {/* Transparent Diagnostic Explanations */}
      {health.active_fault !== 'NONE' ? (
        <div className="alert-banner">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <polygon points="7.86 2 16.14 2 22 7.86 22 16.14 16.14 22 7.86 22 2 16.14 2 7.86 7.86 2" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <div>
            <strong>Active Fault Detected:</strong> {health.active_fault}
            <div style={{ fontSize: '12px', marginTop: '2px' }}>{health.message}</div>
          </div>
        </div>
      ) : (
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '14px', fontSize: '12px', color: 'var(--text-dim)' }}>
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--emerald-valid)" strokeWidth="2.5">
            <polyline points="20 6 9 17 4 12" />
          </svg>
          <span>{health.message}</span>
        </div>
      )}
    </div>
  );
};
