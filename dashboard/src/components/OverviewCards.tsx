import React from 'react';
import { RepetitionData } from '../types';

interface OverviewCardsProps {
  repetition: RepetitionData;
  sessionElapsedSeconds: number;
  onResetReps: () => void;
  onSelectExercise: (profile: 'bicep_curl' | 'wrist_extension' | 'dumbbell_press') => void;
}

export const OverviewCards: React.FC<OverviewCardsProps> = ({
  repetition,
  sessionElapsedSeconds,
  onResetReps,
  onSelectExercise,
}) => {
  const formatTime = (totalSeconds: number) => {
    const mins = Math.floor(totalSeconds / 60);
    const secs = totalSeconds % 60;
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
  };

  return (
    <div className="grid-col-12">
      <div className="metric-counters">
        {/* Valid Reps */}
        <div className="counter-card valid">
          <div className="counter-label">Validated Repetitions</div>
          <div className="counter-value">{repetition.valid_count}</div>
          <div className="counter-sub">Motion & Grip Verified</div>
        </div>

        {/* Rejected Reps */}
        <div className="counter-card rejected">
          <div className="counter-label">Rejected Attempts</div>
          <div className="counter-value">{repetition.rejected_count}</div>
          <div className="counter-sub">Form / ROM / Grip Mismatch</div>
        </div>

        {/* Acceptance Ratio */}
        <div className="counter-card ratio">
          <div className="counter-label">Rep Acceptance Ratio</div>
          <div className="counter-value">{repetition.acceptance_ratio_pct}%</div>
          <div className="counter-sub">Valid / Total Attempts</div>
        </div>

        {/* Session Timer & Exercise */}
        <div className="counter-card session">
          <div className="counter-label">Session Duration</div>
          <div className="counter-value" style={{ fontFamily: 'var(--font-mono)', fontSize: '32px' }}>
            {formatTime(sessionElapsedSeconds)}
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginTop: '6px' }}>
            <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>{repetition.exercise}</span>
            <button
              onClick={onResetReps}
              style={{
                fontSize: '11px',
                color: 'var(--crimson-fault)',
                fontWeight: 600,
                textDecoration: 'underline',
              }}
            >
              Reset
            </button>
          </div>
        </div>
      </div>

      {/* Exercise Profile Switcher & Movement Phase Bar */}
      <div
        style={{
          background: 'var(--bg-card)',
          border: '1px solid var(--border-subtle)',
          borderRadius: 'var(--radius-lg)',
          padding: '12px 20px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '12px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-muted)', textTransform: 'uppercase' }}>
            Exercise Profile:
          </span>
          <div style={{ display: 'flex', gap: '6px' }}>
            {(
              [
                { id: 'bicep_curl', label: 'Bicep Curl' },
                { id: 'wrist_extension', label: 'Wrist Extension' },
                { id: 'dumbbell_press', label: 'Dumbbell Press' },
              ] as const
            ).map((ex) => (
              <button
                key={ex.id}
                onClick={() => onSelectExercise(ex.id)}
                style={{
                  padding: '6px 12px',
                  borderRadius: 'var(--radius-sm)',
                  fontSize: '12px',
                  fontWeight: 600,
                  background: repetition.exercise.toLowerCase().includes(ex.id.split('_')[0])
                    ? 'rgba(0, 229, 255, 0.15)'
                    : 'var(--bg-surface)',
                  color: repetition.exercise.toLowerCase().includes(ex.id.split('_')[0])
                    ? 'var(--cyan-primary)'
                    : 'var(--text-muted)',
                  border: repetition.exercise.toLowerCase().includes(ex.id.split('_')[0])
                    ? '1px solid var(--cyan-primary)'
                    : '1px solid var(--border-subtle)',
                }}
              >
                {ex.label}
              </button>
            ))}
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)' }}>FSM State:</span>
          <div className={`rep-state-badge ${repetition.state}`}>{repetition.state}</div>
        </div>
      </div>
    </div>
  );
};
