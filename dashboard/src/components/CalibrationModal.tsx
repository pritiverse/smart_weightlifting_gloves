import React from 'react';
import { CalibrationSummary, CalibrationProfile } from '../types';

interface CalibrationModalProps {
  isOpen: boolean;
  onClose: () => void;
  calibration: CalibrationSummary;
  profile: CalibrationProfile | null;
  onStartCalibration: () => void;
  onCancelCalibration: () => void;
  onResetCalibration: () => void;
}

export const CalibrationModal: React.FC<CalibrationModalProps> = ({
  isOpen,
  onClose,
  calibration,
  profile,
  onStartCalibration,
  onCancelCalibration,
  onResetCalibration,
}) => {
  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <div className="modal-header">
          <div>
            <h2 className="modal-title">Personal Calibration Wizard</h2>
            <p style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
              Configure individual neutral posture & 3σ grip separation bounds
            </p>
          </div>
          <button onClick={onClose} style={{ fontSize: '20px', color: 'var(--text-dim)', padding: '4px' }}>
            ✕
          </button>
        </div>

        {/* Active Step Indicator */}
        <div
          style={{
            background: 'var(--bg-surface)',
            border: '1px solid var(--border-subtle)',
            borderRadius: 'var(--radius-lg)',
            padding: '18px',
            marginBottom: '20px',
          }}
        >
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--cyan-primary)', textTransform: 'uppercase' }}>
              Current Step: {calibration.step}
            </span>
            <span style={{ fontSize: '12px', fontFamily: 'var(--font-mono)', color: '#fff' }}>
              {calibration.progress_pct}%
            </span>
          </div>

          <div style={{ width: '100%', height: '8px', background: '#090e18', borderRadius: '4px', overflow: 'hidden' }}>
            <div
              style={{
                height: '100%',
                width: `${calibration.progress_pct}%`,
                background: 'linear-gradient(90deg, var(--cyan-primary), var(--emerald-valid))',
                transition: 'width 0.2s ease',
              }}
            />
          </div>

          <div style={{ fontSize: '13px', color: 'var(--text-main)', marginTop: '12px', lineHeight: 1.4 }}>
            {calibration.step === 'PREFLIGHT_CHECK' && 'Hold hand relaxed and still. Checking sensor noise floor...'}
            {calibration.step === 'NEUTRAL_WRIST_HOLD' && 'Hold wrist in comfortable neutral lifting posture for 3.0 seconds.'}
            {calibration.step === 'RELAXED_GRIP_BASELINE' && 'Rest fingers lightly on barbell/handle for 2.0 seconds.'}
            {calibration.step === 'FIRM_GRIP_CAPTURE' && 'Squeeze handle firmly with repeatable exercise pressure for 2.0 seconds.'}
            {calibration.step === 'STATISTICAL_VALIDATION' && 'Calculating Gaussian separation ratio and verifying posture stability...'}
            {calibration.step === 'IDLE' && 'Ready to begin personalized calibration.'}
            {calibration.step === 'COMPLETED' && 'Calibration successfully validated and written to ESP32 flash memory!'}
            {calibration.step === 'FAILED' && 'Calibration rejected due to inconsistent grip or excessive motion. Try again.'}
          </div>
        </div>

        {/* Stored Profile Parameters */}
        {profile && (
          <div style={{ background: 'var(--bg-surface)', padding: '14px', borderRadius: 'var(--radius-md)', marginBottom: '20px' }}>
            <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase', marginBottom: '8px' }}>
              Stored Flash Calibration Profile
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '12px' }}>
              <div>Neutral Pitch: <strong>{profile.neutral_pitch_deg}°</strong></div>
              <div>Relaxed Baseline: <strong>{profile.fsr_relaxed_mean_adc} ADC</strong></div>
              <div>Neutral Roll: <strong>{profile.neutral_roll_deg}°</strong></div>
              <div>Firm Target: <strong>{profile.fsr_firm_mean_adc} ADC</strong></div>
              <div>Separation Ratio: <strong style={{ color: profile.grip_separation_ratio >= 3 ? 'var(--emerald-valid)' : 'var(--crimson-fault)' }}>
                {profile.grip_separation_ratio.toFixed(2)} (Target ≥ 3.0)
              </strong></div>
              <div>Grip Eligibility: <strong>{profile.grip_eligibility_adc} ADC</strong></div>
            </div>
          </div>
        )}

        {/* Modal Controls */}
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '12px' }}>
          <button onClick={onResetCalibration} className="btn-secondary" style={{ color: 'var(--crimson-fault)' }}>
            Reset to Defaults
          </button>
          <div style={{ display: 'flex', gap: '8px' }}>
            {calibration.step !== 'IDLE' && calibration.step !== 'COMPLETED' ? (
              <button onClick={onCancelCalibration} className="btn-secondary">
                Cancel
              </button>
            ) : null}
            <button onClick={onStartCalibration} className="btn-primary">
              {calibration.step === 'IDLE' || calibration.step === 'COMPLETED' || calibration.step === 'FAILED'
                ? 'Start Guided Wizard'
                : 'Restart Wizard'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
