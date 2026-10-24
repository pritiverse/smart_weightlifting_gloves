import React from 'react';
import { RepetitionData } from '../types';

interface MovementTraceProps {
  repetition: RepetitionData;
  historyPitch: number[];
}

export const MovementTrace: React.FC<MovementTraceProps> = ({ repetition, historyPitch }) => {
  // SVG Canvas Dimensions
  const width = 600;
  const height = 180;
  const padding = 20;

  // Scale data points to SVG viewBox
  const minVal = -20;
  const maxVal = 80;
  const range = maxVal - minVal;

  const points = historyPitch.map((val, idx) => {
    const x = padding + (idx / Math.max(1, historyPitch.length - 1)) * (width - 2 * padding);
    const normalizedY = 1 - (val - minVal) / range;
    const y = padding + normalizedY * (height - 2 * padding);
    return `${x},${y}`;
  }).join(' ');

  const romThresholdY = padding + (1 - (40.0 - minVal) / range) * (height - 2 * padding);

  return (
    <div className="dashboard-card grid-col-7">
      <div className="card-header">
        <h2 className="card-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--cyan-primary)" strokeWidth="2.5">
            <polyline points="22 12 18 12 15 21 9 3 6 12 2 12" />
          </svg>
          Real-Time Movement Trajectory (Pitch Excursion)
        </h2>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Peak: {repetition.last_rep_rom_deg}°</span>
          <span style={{ fontSize: '11px', color: 'var(--cyan-primary)', fontFamily: 'var(--font-mono)' }}>
            Excur: {repetition.current_excursion_deg}°
          </span>
        </div>
      </div>

      <div className="chart-container">
        <svg viewBox={`0 0 ${width} ${height}`} style={{ width: '100%', height: '100%' }}>
          {/* Grid lines */}
          <line x1={padding} y1={height / 2} x2={width - padding} y2={height / 2} stroke="#1c2538" strokeDasharray="4 4" />
          
          {/* ROM Target Line (40 deg) */}
          <line x1={padding} y1={romThresholdY} x2={width - padding} y2={romThresholdY} stroke="rgba(0, 230, 118, 0.4)" strokeDasharray="5 5" />
          <text x={width - padding - 65} y={romThresholdY - 6} fill="var(--emerald-valid)" fontSize="9" fontWeight="700">
            MIN ROM (40°)
          </text>

          {/* Real-time Movement Path */}
          {historyPitch.length > 1 && (
            <polyline
              fill="none"
              stroke="var(--cyan-primary)"
              strokeWidth="2.5"
              strokeLinecap="round"
              strokeLinejoin="round"
              points={points}
            />
          )}
        </svg>
      </div>

      {/* Rejection / Rep Quality Explanation Callout */}
      {repetition.last_rejection_reason && repetition.last_rejection_reason !== 'NONE (VALID)' && (
        <div className="alert-banner warning" style={{ marginTop: '12px' }}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <div>
            <strong>Last Attempt Classified:</strong> {repetition.last_rejection_reason}.
            <div style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '2px' }}>
              Duration: {repetition.last_rep_duration_ms}ms | Peak ROM: {repetition.last_rep_rom_deg}° | Grip: {repetition.last_rep_grip_pct}%
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
