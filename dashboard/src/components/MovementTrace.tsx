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

  const activeAdaptiveRom = repetition.adaptive_min_rom_deg ?? 40.0;

  // Scale data points to SVG viewBox for 3D Excursion (0 deg to 120+ deg)
  const minVal = 0;
  const maxVal = Math.max(120, activeAdaptiveRom + 20, ...(historyPitch.length > 0 ? historyPitch : [120]));
  const range = maxVal - minVal;

  const points = historyPitch.map((val, idx) => {
    const x = padding + (idx / Math.max(1, historyPitch.length - 1)) * (width - 2 * padding);
    const clampedVal = Math.max(minVal, Math.min(maxVal, val));
    const normalizedY = 1 - (clampedVal - minVal) / range;
    const y = padding + normalizedY * (height - 2 * padding);
    return `${x},${y}`;
  }).join(' ');

  const adaptiveThresholdY = padding + (1 - (activeAdaptiveRom - minVal) / range) * (height - 2 * padding);
  const nominalThresholdY = padding + (1 - (40.0 - minVal) / range) * (height - 2 * padding);

  const getExplanationDetail = (reason: string) => {
    if (reason.includes('UNLOADED')) {
      return 'Inertial load decoupled from hand grip (ρ < 0.12). True dumbbell/barbell resistance was not detected.';
    }
    if (reason.includes('MOMENTUM_SWING')) {
      return 'Torso momentum swing cheat detected (<450ms explosive acceleration with sensor decoupling).';
    }
    if (reason.includes('INSUFFICIENT_ROM') || reason.includes('INSUFFICIENT_RANGE')) {
      return `Movement excursion (${repetition.last_rep_rom_deg}°) failed to reach the required threshold (${activeAdaptiveRom.toFixed(1)}°).`;
    }
    if (reason.includes('INSUFFICIENT_GRIP')) {
      if (repetition.last_rep_grip_pct >= 35) {
        return `Grip momentarily dropped below gate threshold during movement (average palm grip was ${repetition.last_rep_grip_pct}%).`;
      }
      return `Grip pressure (${repetition.last_rep_grip_pct}%) fell below minimum threshold (35%) during lift.`;
    }
    if (reason.includes('DURATION_TOO_LONG') || reason.includes('STALLED')) {
      return `Movement duration (${repetition.last_rep_duration_ms}ms) exceeded maximum allowable time window (rep stalled).`;
    }
    if (reason.includes('DURATION_TOO_SHORT')) {
      return `Movement (${repetition.last_rep_duration_ms}ms) was too fast (<700ms), indicating ballistic jerk without muscle control.`;
    }
    if (reason.includes('INCOMPLETE_RETURN')) {
      return 'Arm did not return within tolerance of starting baseline before movement ended.';
    }
    return 'Repetition parameters deviated from biomechanical validity criteria.';
  };

  return (
    <div className="dashboard-card grid-col-7">
      <div className="card-header">
        <h2 className="card-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--cyan-primary)" strokeWidth="2.5">
            <polyline points="22 12 18 12 15 21 9 3 6 12 2 12" />
          </svg>
          Real-Time Forearm 3D Excursion Trace
        </h2>
        <div style={{ display: 'flex', gap: '10px', alignItems: 'center' }}>
          <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Peak: {repetition.last_rep_rom_deg}°</span>
          <span style={{ fontSize: '11px', color: 'var(--cyan-primary)', fontFamily: 'var(--font-mono)' }}>
            Excur: {repetition.current_excursion_deg}°
          </span>
          <span style={{ fontSize: '10px', padding: '2px 6px', borderRadius: '4px', background: 'rgba(0, 229, 255, 0.1)', color: 'var(--cyan-primary)' }}>
            Mahony 6-DoF
          </span>
        </div>
      </div>

      <div className="chart-container">
        <svg viewBox={`0 0 ${width} ${height}`} style={{ width: '100%', height: '100%' }}>
          {/* Grid lines */}
          <line x1={padding} y1={height / 2} x2={width - padding} y2={height / 2} stroke="#1c2538" strokeDasharray="4 4" />
          
          {/* Nominal ROM Line (40 deg) */}
          <line x1={padding} y1={nominalThresholdY} x2={width - padding} y2={nominalThresholdY} stroke="rgba(255, 255, 255, 0.15)" strokeDasharray="3 3" />
          <text x={padding + 6} y={nominalThresholdY - 4} fill="var(--text-dim)" fontSize="9">
            Nominal Min (40°)
          </text>

          {/* Dynamic Adaptive ROM Target Line */}
          <line x1={padding} y1={adaptiveThresholdY} x2={width - padding} y2={adaptiveThresholdY} stroke="rgba(0, 230, 118, 0.6)" strokeDasharray="5 5" />
          <text x={width - padding - 130} y={adaptiveThresholdY - 6} fill="var(--emerald-valid)" fontSize="9" fontWeight="700">
            Adaptive Target ({activeAdaptiveRom.toFixed(1)}°)
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
      {repetition.last_rejection_reason && repetition.last_rejection_reason !== 'NONE (VALID)' && repetition.last_rejection_reason !== 'NONE' && (
        <div className="alert-banner warning" style={{ marginTop: '12px' }}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <div>
            <strong>Last Attempt Classified:</strong> {repetition.last_rejection_reason}.
            <div style={{ fontSize: '12px', color: '#fca5a5', marginTop: '3px' }}>
              {getExplanationDetail(repetition.last_rejection_reason)}
            </div>
            <div style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '4px' }}>
              Duration: {repetition.last_rep_duration_ms}ms | Peak ROM: {repetition.last_rep_rom_deg}° | Palm Grip: {repetition.last_rep_grip_pct}% | Load Coupling (ρ): {(repetition.coupling_rho ?? 0).toFixed(3)}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
