import React from 'react';
import { RepetitionData } from '../types';

interface OverviewCardsProps {
  repetition: RepetitionData;
  sessionElapsedSeconds: number;
  sessionActive: boolean;
  onStartSession: () => void;
  onStopSession: () => void;
  onResetReps: () => void;
  onSelectExercise: (profile: 'bicep_curl' | 'wrist_extension' | 'dumbbell_press') => void;
}

export const OverviewCards: React.FC<OverviewCardsProps> = ({
  repetition,
  sessionElapsedSeconds,
  sessionActive,
  onStartSession,
  onStopSession,
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

      {/* ── Session Control Bar ── */}
      <div style={{
        background: sessionActive
          ? 'linear-gradient(135deg, rgba(0,229,255,0.08) 0%, rgba(0,180,200,0.04) 100%)'
          : 'linear-gradient(135deg, rgba(255,255,255,0.03) 0%, rgba(255,255,255,0.01) 100%)',
        border: `1px solid ${sessionActive ? 'var(--cyan-primary)' : 'var(--border-subtle)'}`,
        borderRadius: 'var(--radius-lg)',
        padding: '16px 24px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        gap: '16px',
        flexWrap: 'wrap',
        marginBottom: '12px',
        transition: 'all 0.3s ease',
      }}>
        {/* Left: Session state info */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          {/* Animated live indicator */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{
              width: '10px', height: '10px', borderRadius: '50%',
              background: sessionActive ? 'var(--cyan-primary)' : 'var(--text-muted)',
              boxShadow: sessionActive ? '0 0 8px var(--cyan-primary)' : 'none',
              animation: sessionActive ? 'pulse 1.5s infinite' : 'none',
              flexShrink: 0,
            }} />
            <span style={{
              fontSize: '13px',
              fontWeight: 700,
              color: sessionActive ? 'var(--cyan-primary)' : 'var(--text-muted)',
              textTransform: 'uppercase',
              letterSpacing: '0.05em',
            }}>
              {sessionActive ? 'Session Active' : 'Session Paused'}
            </span>
          </div>
          {sessionActive && (
            <div style={{
              fontFamily: 'var(--font-mono)',
              fontSize: '20px',
              fontWeight: 700,
              color: '#fff',
              letterSpacing: '0.05em',
            }}>
              {formatTime(sessionElapsedSeconds)}
            </div>
          )}
        </div>

        {/* Right: Controls */}
        <div style={{ display: 'flex', gap: '10px', alignItems: 'center' }}>
          {!sessionActive ? (
            <button
              onClick={onStartSession}
              style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '10px 24px',
                borderRadius: '10px',
                fontSize: '14px',
                fontWeight: 700,
                background: 'var(--cyan-primary)',
                color: '#040814',
                border: 'none',
                cursor: 'pointer',
                boxShadow: '0 0 20px rgba(0,229,255,0.35)',
                transition: 'all 0.2s',
                letterSpacing: '0.03em',
              }}
            >
              <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
                <polygon points="5,3 19,12 5,21"/>
              </svg>
              Start Session
            </button>
          ) : (
            <button
              onClick={onStopSession}
              style={{
                display: 'flex', alignItems: 'center', gap: '8px',
                padding: '10px 24px',
                borderRadius: '10px',
                fontSize: '14px',
                fontWeight: 700,
                background: 'rgba(239,68,68,0.15)',
                color: '#f87171',
                border: '1px solid rgba(239,68,68,0.4)',
                cursor: 'pointer',
                transition: 'all 0.2s',
                letterSpacing: '0.03em',
              }}
            >
              <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
                <rect x="3" y="3" width="18" height="18" rx="2"/>
              </svg>
              Stop Session
            </button>
          )}

          <button
            onClick={onResetReps}
            title="Reset rep counters & timer"
            style={{
              padding: '10px 14px',
              borderRadius: '10px',
              fontSize: '12px',
              fontWeight: 600,
              background: 'var(--bg-surface)',
              color: 'var(--text-muted)',
              border: '1px solid var(--border-subtle)',
              cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '6px',
              transition: 'all 0.2s',
            }}
          >
            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
              <polyline points="1 4 1 10 7 10"/>
              <path d="M3.51 15a9 9 0 1 0 .49-4.95"/>
            </svg>
            Reset
          </button>
        </div>
      </div>

      {/* ── Rep Counter Cards ── */}
      <div className="metric-counters">
        {/* Valid Reps */}
        <div className="counter-card valid" style={{ opacity: sessionActive ? 1 : 0.5, transition: 'opacity 0.3s' }}>
          <div className="counter-label">Repetitions Detected</div>
          <div className="counter-value">{repetition.valid_count}</div>
          <div className="counter-sub">Motion &amp; Palm Pressure Verified</div>
        </div>

        {/* Rejected Reps */}
        <div className="counter-card rejected" style={{ opacity: sessionActive ? 1 : 0.5, transition: 'opacity 0.3s' }}>
          <div className="counter-label">Rejected Attempts</div>
          <div className="counter-value">{repetition.rejected_count}</div>
          <div className="counter-sub">Form / ROM / Grip Mismatch</div>
        </div>

        {/* Acceptance Ratio */}
        <div className="counter-card ratio" style={{ opacity: sessionActive ? 1 : 0.5, transition: 'opacity 0.3s' }}>
          <div className="counter-label">Rep Acceptance Ratio</div>
          <div className="counter-value">{repetition.acceptance_ratio_pct}%</div>
          <div className="counter-sub">Valid / Total Attempts</div>
        </div>

        {/* Rep State */}
        <div className="counter-card session">
          <div className="counter-label">Current Phase</div>
          <div style={{ margin: '8px 0' }}>
            <div className={`rep-state-badge ${repetition.state}`} style={{ fontSize: '14px', padding: '6px 14px' }}>
              {repetition.state}
            </div>
          </div>
          <div className="counter-sub">{repetition.exercise}</div>
        </div>
      </div>

      {/* ── Exercise Profile Switcher ── */}
      <div style={{
        background: 'var(--bg-card)',
        border: '1px solid var(--border-subtle)',
        borderRadius: 'var(--radius-lg)',
        padding: '12px 20px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexWrap: 'wrap',
        gap: '12px',
      }}>
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
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                }}
              >
                {ex.label}
              </button>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
