import React, { useState, useEffect, useRef } from 'react';
import { CalibrationSummary, CalibrationProfile } from '../types';

interface CalibrationModalProps {
  isOpen: boolean;
  onClose: () => void;
  calibration: CalibrationSummary;
  profile: CalibrationProfile | null;
  onStartCalibration: () => void;
  onNextStep?: () => void;
  onCancelCalibration: () => void;
  onResetCalibration: () => void;
}

// ── Web Audio Feedback Helper ────────────────────────────────────────────────
const playBeep = (freq = 880, durationMs = 80, enabled = true) => {
  if (!enabled) return;
  try {
    const AudioCtx = window.AudioContext || (window as unknown as { webkitAudioContext: typeof window.AudioContext }).webkitAudioContext;
    if (!AudioCtx) return;
    const ctx = new AudioCtx();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(freq, ctx.currentTime);
    gain.gain.setValueAtTime(0.12, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + durationMs / 1000);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + durationMs / 1000);
  } catch {
    // Audio context may be restricted by browser until user gesture
  }
};

const playCelebration = (enabled = true) => {
  if (!enabled) return;
  try {
    playBeep(1046.5, 90, enabled);
    setTimeout(() => playBeep(1318.5, 110, enabled), 110);
    setTimeout(() => playBeep(1567.98, 220, enabled), 240);
  } catch {
    // Ignore audio restriction
  }
};

// ── Step Definitions ─────────────────────────────────────────────────────────
interface StepMeta {
  number: number;
  id: string;
  firmwareSample: string;
  firmwarePrep: string;
  title: string;
  emoji: string;
  color: string;
  prepDuration: number; // seconds
  sampleDuration: number; // seconds
  instruction: string;
  detail: string;
  prepTip: string;
  illustration: React.ReactNode;
}

const STEPS: StepMeta[] = [
  {
    number: 1,
    id: 'rest_arm',
    firmwarePrep: 'PREP_REST_ARM',
    firmwareSample: 'PREFLIGHT_CHECK',
    title: 'Rest Your Arm',
    emoji: '🤚',
    color: '#60a5fa',
    prepDuration: 5, // 5 seconds initial preparation
    sampleDuration: 1.5,
    instruction: 'Arm hanging straight down at your side, hold completely still.',
    detail: 'The glove calibrates the resting electronic noise floor so involuntary tremors are ignored.',
    prepTip: 'Stand upright or sit comfortably. Let your arm hang limp and relaxed.',
    illustration: <img src="/calib_step1.jpg" alt="Arm hanging relaxed" style={{ width: '160px', height: '160px', objectFit: 'cover', borderRadius: '12px', display: 'block', margin: '0 auto', boxShadow: '0 4px 16px rgba(0,0,0,0.4)' }} />,
  },
  {
    number: 2,
    id: 'starting_position',
    firmwarePrep: 'PREP_START_POSITION',
    firmwareSample: 'NEUTRAL_WRIST_HOLD',
    title: 'Hold Starting Position',
    emoji: '🏋️',
    color: '#00e5ff',
    prepDuration: 3, // 3 seconds transition buffer
    sampleDuration: 3,
    instruction: 'Dumbbell in hand at natural pre-curl angle. Hold still for 3 seconds.',
    detail: 'This sets your "zero point" baseline — the angle treated as the bottom of every rep.',
    prepTip: 'Pick up your dumbbell now and steady your wrist in starting pre-curl stance.',
    illustration: <img src="/calib_step2.jpg" alt="Dumbbell in natural pre-curl position" style={{ width: '160px', height: '160px', objectFit: 'cover', borderRadius: '12px', display: 'block', margin: '0 auto', boxShadow: '0 4px 16px rgba(0,0,0,0.4)' }} />,
  },
  {
    number: 3,
    id: 'relaxed_grip',
    firmwarePrep: 'PREP_RELAX_GRIP',
    firmwareSample: 'RELAXED_GRIP_BASELINE',
    title: 'Relax Grip ("Egg Shell")',
    emoji: '🥚',
    color: '#a78bfa',
    prepDuration: 3, // 3 seconds transition buffer
    sampleDuration: 2.5,
    instruction: 'Hold the dumbbell lightly with fingers resting gently like an eggshell. Do not squeeze.',
    detail: 'This teaches the glove what "unloaded contact" feels like so it can detect true grip onset.',
    prepTip: 'Hold the dumbbell, but keep your fingers as relaxed and gentle as possible ("hold an egg without breaking it").',
    illustration: <img src="/calib_step3.jpg" alt="Barely touching the bar - hold an egg" style={{ width: '180px', height: '180px', objectFit: 'cover', borderRadius: '12px', display: 'block', margin: '0 auto', boxShadow: '0 4px 16px rgba(0,0,0,0.4)' }} />,
  },
  {
    number: 4,
    id: 'firm_grip',
    firmwarePrep: 'PREP_FIRM_GRIP',
    firmwareSample: 'FIRM_GRIP_CAPTURE',
    title: 'Squeeze Firmly',
    emoji: '💪',
    color: '#00e5ff',
    prepDuration: 3, // 3 seconds transition buffer
    sampleDuration: 2,
    instruction: 'Squeeze as hard as you would during a real rep. Hold for 2 seconds.',
    detail: 'This captures your true dynamic isometric squeeze reference threshold.',
    prepTip: 'Get ready to squeeze the dumbbell firmly with full intent.',
    illustration: <img src="/calib_step4.jpg" alt="Squeeze firmly" style={{ width: '160px', height: '160px', objectFit: 'cover', borderRadius: '12px', display: 'block', margin: '0 auto', boxShadow: '0 4px 16px rgba(0,0,0,0.4)' }} />,
  },
  {
    number: 5,
    id: 'validation_commit',
    firmwarePrep: 'PREP_VALIDATE',
    firmwareSample: 'VALIDATION_AND_COMMIT',
    title: 'Validate & Commit',
    emoji: '💾',
    color: '#10b981',
    prepDuration: 2,
    sampleDuration: 1.5,
    instruction: 'Verifying separation ratio (Sr >= 3.0) and committing profile to NVS memory.',
    detail: 'Calculates Gaussian separation ratio, IEEE 802.3 CRC32 checksum, and permanently activates workout profiling.',
    prepTip: 'Hold steady while the microcontroller validates statistical integrity and writes to flash memory.',
    illustration: <div style={{ width: '160px', height: '160px', borderRadius: '12px', background: 'rgba(16, 185, 129, 0.15)', border: '2px solid #10b981', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', margin: '0 auto', gap: '8px' }}><span style={{ fontSize: '48px' }}>💾</span><span style={{ fontSize: '12px', fontWeight: 800, color: '#10b981', letterSpacing: '0.04em' }}>NVS COMMITTED</span></div>,
  },
];

export const CalibrationModal: React.FC<CalibrationModalProps> = ({
  isOpen,
  onClose,
  calibration,
  profile,
  onStartCalibration,
  onNextStep,
  onCancelCalibration,
  onResetCalibration,
}) => {
  // Sound toggle
  const [soundEnabled, setSoundEnabled] = useState<boolean>(true);

  // Client-managed countdown buffer for transitions
  const [prepCountdown, setPrepCountdown] = useState<number | null>(null);
  const [activeStepIndex, setActiveStepIndex] = useState<number>(0);
  const [isSampling, setIsSampling] = useState<boolean>(false);
  const [justCompletedStep, setJustCompletedStep] = useState<number | null>(null);

  const prevStepRef = useRef<string>(calibration.step);
  const countdownIntervalRef = useRef<number | null>(null);

  // Cleanup on unmount or close
  useEffect(() => {
    return () => {
      if (countdownIntervalRef.current) {
        window.clearInterval(countdownIntervalRef.current);
      }
    };
  }, []);

  // Determine global wizard status
  const isIdle = calibration.step === 'IDLE';
  const isDone = calibration.step === 'COMPLETED';
  const isFailed = calibration.step === 'FAILED';
  const isRunning = !isIdle && !isDone && !isFailed;

  // Track firmware step changes to trigger inter-step preparation and celebration
  useEffect(() => {
    const prev = prevStepRef.current;
    const curr = calibration.step;
    prevStepRef.current = curr;

    if (!isRunning) {
      if (countdownIntervalRef.current) {
        window.clearInterval(countdownIntervalRef.current);
        countdownIntervalRef.current = null;
      }
      setPrepCountdown(null);
      setIsSampling(false);
      if (isDone) {
        playCelebration(soundEnabled);
      }
      return;
    }

    // Check if an active sampling step just successfully finished
    const prevSampleIdx = STEPS.findIndex(s => s.firmwareSample === prev);
    if (prevSampleIdx !== -1 && (curr.startsWith('PREP_') || curr === 'COMPLETED' || curr === 'VALIDATION_AND_COMMIT')) {
      const finishedStepNum = STEPS[prevSampleIdx].number;
      setJustCompletedStep(finishedStepNum);
      playCelebration(soundEnabled);
    }

    // Firmware is in an explicit PREP step (waiting for user readiness)
    if (curr.startsWith('PREP_')) {
      setIsSampling(false);
      let stepIdx = 0;
      if (curr === 'PREP_REST_ARM') stepIdx = 0;
      else if (curr === 'PREP_START_POSITION') stepIdx = 1;
      else if (curr === 'PREP_RELAX_GRIP') stepIdx = 2;
      else if (curr === 'PREP_FIRM_GRIP') stepIdx = 3;
      else if (curr === 'PREP_VALIDATE') stepIdx = 4;

      setActiveStepIndex(stepIdx);
      // User-paced: DO NOT auto-countdown! Wait for user click.
      setPrepCountdown(null);
      return;
    }

    // Firmware entered an active sample step
    const matchedSampleStepIdx = STEPS.findIndex(s => s.firmwareSample === curr);
    if (matchedSampleStepIdx !== -1) {
      setActiveStepIndex(matchedSampleStepIdx);
      setIsSampling(true);
      setPrepCountdown(null);
    }
  }, [calibration.step, isRunning, isDone, soundEnabled]);

  // Handle user confirming readiness for current step
  const handleReadyClick = () => {
    setPrepCountdown(3);
    playBeep(880, 80, soundEnabled);

    if (countdownIntervalRef.current) {
      window.clearInterval(countdownIntervalRef.current);
    }

    let remaining = 3;
    countdownIntervalRef.current = window.setInterval(() => {
      remaining -= 1;
      if (remaining > 0) {
        setPrepCountdown(remaining);
        playBeep(880, 80, soundEnabled);
      } else {
        setPrepCountdown(null);
        setIsSampling(true);
        playBeep(1320, 160, soundEnabled);
        if (countdownIntervalRef.current) {
          window.clearInterval(countdownIntervalRef.current);
          countdownIntervalRef.current = null;
        }
        // Tell firmware to start active sampling
        if (onNextStep) {
          onNextStep();
        }
      }
    }, 1000);
  };

  if (!isOpen) return null;

  const currentStep = STEPS[activeStepIndex] || STEPS[0];
  const stepNumber = currentStep.number;
  const isPrepState = prepCountdown !== null && prepCountdown > 0;
  const isAwaitingUser = isRunning && calibration.step.startsWith('PREP_') && prepCountdown === null;

  return (
    <div className="modal-overlay">
      <div className="modal-content" style={{ maxWidth: '560px', maxHeight: '92vh', overflowY: 'auto' }}>
        
        {/* Header */}
        <div className="modal-header" style={{ marginBottom: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div>
            <h2 className="modal-title" style={{ fontSize: '18px', fontWeight: 800, letterSpacing: '-0.02em', display: 'flex', alignItems: 'center', gap: '8px' }}>
              ⚙️ Personal Calibration
            </h2>
            <p style={{ fontSize: '12px', color: 'var(--text-muted)', marginTop: '3px' }}>
              5 guided steps · 5s initial prep · 3s transition gaps
            </p>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <button
              onClick={() => setSoundEnabled(!soundEnabled)}
              title={soundEnabled ? 'Mute audio countdown' : 'Enable audio countdown'}
              style={{
                background: soundEnabled ? 'rgba(0,229,255,0.1)' : 'var(--bg-surface)',
                border: `1px solid ${soundEnabled ? 'var(--cyan-primary)' : 'var(--border-subtle)'}`,
                borderRadius: '8px',
                padding: '5px 10px',
                fontSize: '12px',
                color: soundEnabled ? 'var(--cyan-primary)' : 'var(--text-dim)',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              {soundEnabled ? '🔔 Audio On' : '🔕 Muted'}
            </button>
            <button
              onClick={onClose}
              style={{ fontSize: '20px', color: 'var(--text-dim)', padding: '4px', background: 'none', border: 'none', cursor: 'pointer' }}
            >
              ✕
            </button>
          </div>
        </div>

        {/* ── ROADMAP BAR (Always visible for orientation) ── */}
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '22px', position: 'relative' }}>
          <div style={{ position: 'absolute', top: '16px', left: '10%', right: '10%', height: '2px', background: 'var(--border-subtle)', zIndex: 0 }} />
          {isRunning && (
            <div style={{
              position: 'absolute', top: '16px', left: '10%',
              width: `${((stepNumber - 1) / (STEPS.length - 1)) * 80}%`,
              height: '2px', background: 'var(--cyan-primary)', zIndex: 0, transition: 'width 0.4s ease',
            }} />
          )}
          {STEPS.map((s) => {
            const isCompleted = isDone || (isRunning && s.number < stepNumber);
            const isCurrent = isRunning && s.number === stepNumber;
            return (
              <div key={s.number} style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '5px', zIndex: 1, flex: 1 }}>
                <div style={{
                  width: '32px', height: '32px', borderRadius: '50%', display: 'flex', alignItems: 'center', justifyContent: 'center',
                  background: isCompleted ? 'rgba(0,230,118,0.15)' : isCurrent ? 'rgba(0,229,255,0.15)' : 'var(--bg-surface)',
                  border: `2px solid ${isCompleted ? 'var(--emerald-valid)' : isCurrent ? 'var(--cyan-primary)' : 'var(--border-subtle)'}`,
                  fontSize: isCompleted ? '15px' : isCurrent ? '14px' : '12px',
                  fontWeight: 700,
                  transition: 'all 0.3s',
                  boxShadow: isCurrent ? '0 0 12px rgba(0,229,255,0.3)' : 'none',
                }}>
                  {isCompleted ? '✓' : s.emoji}
                </div>
                <span style={{
                  fontSize: '9px',
                  fontWeight: isCurrent ? 700 : 500,
                  color: isCurrent ? 'var(--cyan-primary)' : isCompleted ? 'var(--emerald-valid)' : 'var(--text-muted)',
                  textAlign: 'center',
                  maxWidth: '70px',
                  lineHeight: 1.25,
                }}>
                  {s.title}
                </span>
              </div>
            );
          })}
        </div>

        {/* ── IDLE / DONE / FAILED STATES ── */}
        {(isIdle || isDone || isFailed) && (
          <div>
            {isDone && (
              <div style={{ background: 'rgba(0,230,118,0.08)', border: '1px solid rgba(0,230,118,0.3)', borderRadius: '12px', padding: '18px', marginBottom: '18px', textAlign: 'center' }}>
                <div style={{ fontSize: '36px', marginBottom: '6px' }}>✅</div>
                <div style={{ fontWeight: 800, color: 'var(--emerald-valid)', fontSize: '16px' }}>Personal Calibration Saved!</div>
                <div style={{ fontSize: '12px', color: 'var(--text-muted)', marginTop: '4px', lineHeight: 1.6 }}>
                  Neutral wrist angle and grip thresholds are stored in glove memory across restarts.
                </div>
              </div>
            )}

            {isFailed && (
              <div style={{ background: 'rgba(239,68,68,0.08)', border: '1px solid rgba(239,68,68,0.3)', borderRadius: '12px', padding: '18px', marginBottom: '18px', textAlign: 'center' }}>
                <div style={{ fontSize: '36px', marginBottom: '6px' }}>❌</div>
                <div style={{ fontWeight: 800, color: 'var(--crimson-fault)', fontSize: '16px' }}>Calibration Interrupted or Inconsistent</div>
                {calibration.failure_reason && (
                  <div style={{
                    margin: '10px 0',
                    background: 'rgba(239, 68, 68, 0.15)',
                    border: '1px solid rgba(239, 68, 68, 0.4)',
                    borderRadius: '8px',
                    padding: '8px 12px',
                    fontSize: '12px',
                    color: '#fca5a5',
                    fontWeight: 600,
                    textAlign: 'center',
                    fontFamily: 'monospace'
                  }}>
                    ⚠️ {calibration.failure_reason}
                  </div>
                )}
                <div style={{ fontSize: '12px', color: 'var(--text-muted)', marginTop: '6px', lineHeight: 1.6 }}>
                  {calibration.step === 'FAILED' ? 'The sensor detected movement or insufficient grip dynamic range.' : 'Sensor reading timed out.'}<br />
                  <strong style={{ color: '#fff' }}>Tip:</strong> Use the transition countdown to steady your hand before each measurement starts!
                </div>
              </div>
            )}

            {isIdle && (
              <div style={{ background: 'var(--bg-surface)', borderRadius: '12px', padding: '16px', marginBottom: '18px', border: '1px solid var(--border-subtle)' }}>
                <div style={{ fontWeight: 700, fontSize: '13px', color: '#fff', marginBottom: '10px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  📋 What happens in this calibration:
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', fontSize: '11px', color: 'var(--text-muted)' }}>
                  <div style={{ background: 'rgba(0,0,0,0.2)', padding: '10px', borderRadius: '8px' }}>
                    <div style={{ fontWeight: 700, color: '#60a5fa', marginBottom: '3px' }}>1. 🤚 Rest Arm</div>
                    Arm limp at side. 5s prep gives you time to relax.
                  </div>
                  <div style={{ background: 'rgba(0,0,0,0.2)', padding: '10px', borderRadius: '8px' }}>
                    <div style={{ fontWeight: 700, color: '#00e5ff', marginBottom: '3px' }}>2. 🏋️ Starting Posture</div>
                    Dumbbell in hand at natural curl angle.
                  </div>
                  <div style={{ background: 'rgba(0,0,0,0.2)', padding: '10px', borderRadius: '8px' }}>
                    <div style={{ fontWeight: 700, color: '#a78bfa', marginBottom: '3px' }}>3. 🥚 Relaxed Grip</div>
                    Barely touching the bar ("hold an egg").
                  </div>
                  <div style={{ background: 'rgba(0,0,0,0.2)', padding: '10px', borderRadius: '8px' }}>
                    <div style={{ fontWeight: 700, color: '#00e5ff', marginBottom: '3px' }}>4. 💪 Squeeze Firmly</div>
                    Squeeze as hard as during an actual rep.
                  </div>
                  <div style={{ background: 'rgba(0,0,0,0.2)', padding: '10px', borderRadius: '8px', gridColumn: '1 / -1' }}>
                    <div style={{ fontWeight: 700, color: '#10b981', marginBottom: '3px' }}>5. 💾 Validate & Commit</div>
                    Calculates statistical separation ratio (Sr &ge; 3.0) and commits profile with CRC32 to NVS memory.
                  </div>
                </div>
                <div style={{ fontSize: '11px', color: 'var(--cyan-primary)', marginTop: '12px', background: 'rgba(0,229,255,0.06)', padding: '8px 12px', borderRadius: '6px' }}>
                  ⏳ <strong>HCI Transition Time:</strong> You get a 5-second countdown to start, followed by 3 seconds between every position so you never have to rush.
                </div>
              </div>
            )}

            {/* Saved Profile Summary */}
            {profile?.is_valid && (
              <div style={{ background: 'rgba(0,0,0,0.3)', padding: '12px 16px', borderRadius: '10px', marginBottom: '18px', fontSize: '12px', border: '1px solid var(--border-subtle)' }}>
                <div style={{ color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase', fontSize: '10px', letterSpacing: '0.05em', marginBottom: '6px' }}>Stored Hardware Profile</div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                  <div>Neutral Pitch: <strong style={{ color: '#fff' }}>{profile.neutral_pitch_deg}°</strong></div>
                  <div>Relaxed Grip: <strong style={{ color: '#fff' }}>{profile.fsr_relaxed_mean_adc} ADC</strong></div>
                  <div>Neutral Roll: <strong style={{ color: '#fff' }}>{profile.neutral_roll_deg}°</strong></div>
                  <div>Firm Grip: <strong style={{ color: '#fff' }}>{profile.fsr_firm_mean_adc} ADC</strong></div>
                  <div style={{ gridColumn: '1/-1', marginTop: '4px' }}>
                    Separation Ratio:{' '}
                    <strong style={{ color: profile.grip_separation_ratio >= 3 ? 'var(--emerald-valid)' : 'var(--crimson-fault)' }}>
                      {profile.grip_separation_ratio.toFixed(2)} {profile.grip_separation_ratio >= 3 ? '✓ Statistically Reliable' : '✗ Too low — please recalibrate'}
                    </strong>
                  </div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* ── RUNNING STATE (ACTIVE STEP OR PREPARATION COUNTDOWN) ── */}
        {isRunning && (
          <div>
            {/* Step Status Badge */}
            <div style={{ display: 'flex', justifyContent: 'center', marginBottom: '14px' }}>
              {isPrepState ? (
                <div style={{
                  background: 'rgba(251,191,36,0.15)',
                  border: '1px solid rgba(251,191,36,0.5)',
                  color: '#fbbf24',
                  fontSize: '12px',
                  fontWeight: 800,
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase',
                  padding: '6px 14px',
                  borderRadius: '20px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  animation: 'pulse 1.5s infinite',
                }}>
                  <span>⏳ COUNTDOWN — GET READY TO HOLD STILL</span>
                </div>
              ) : isAwaitingUser ? (
                <div style={{
                  background: 'rgba(0,229,255,0.12)',
                  border: '1px solid rgba(0,229,255,0.4)',
                  color: 'var(--cyan-primary)',
                  fontSize: '12px',
                  fontWeight: 800,
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase',
                  padding: '6px 14px',
                  borderRadius: '20px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                }}>
                  <span>📍 STEP {currentStep.number} PREPARATION — TAKE YOUR TIME</span>
                </div>
              ) : (
                <div style={{
                  background: isSampling ? 'rgba(0,230,118,0.15)' : 'rgba(0,229,255,0.15)',
                  border: `1px solid ${isSampling ? 'rgba(0,230,118,0.5)' : 'rgba(0,229,255,0.5)'}`,
                  color: isSampling ? 'var(--emerald-valid)' : 'var(--cyan-primary)',
                  fontSize: '12px',
                  fontWeight: 800,
                  letterSpacing: '0.06em',
                  textTransform: 'uppercase',
                  padding: '6px 14px',
                  borderRadius: '20px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                }}>
                  <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: isSampling ? 'var(--emerald-valid)' : 'var(--cyan-primary)', animation: 'pulse 1s infinite' }} />
                  <span>{isSampling ? 'RECORDING SENSORS — HOLD STILL' : 'CALIBRATING SENSORS…'}</span>
                </div>
              )}
            </div>

            {/* Celebratory Banner for Completed Step */}
            {justCompletedStep !== null && isAwaitingUser && (
              <div style={{
                background: 'rgba(16, 185, 129, 0.15)',
                border: '1px solid rgba(16, 185, 129, 0.4)',
                borderRadius: '10px',
                padding: '10px 16px',
                marginBottom: '14px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                color: '#34d399',
                fontWeight: 800,
                fontSize: '13px',
              }}>
                <span>🎉</span>
                <span>Good Job! Step {justCompletedStep} Completed Successfully!</span>
              </div>
            )}

            {/* Main Interactive Card */}
            <div style={{
              background: 'var(--bg-surface)',
              border: `1px solid ${isPrepState ? 'rgba(251,191,36,0.4)' : currentStep.color + '40'}`,
              borderRadius: '16px',
              padding: '22px',
              textAlign: 'center',
              marginBottom: '16px',
              position: 'relative',
              overflow: 'hidden',
              boxShadow: isPrepState ? '0 0 24px rgba(251,191,36,0.1)' : '0 0 24px rgba(0,229,255,0.08)',
            }}>
              
              {/* Illustration with Countdown Overlay if in Prep */}
              <div style={{ position: 'relative', width: '180px', height: '180px', margin: '0 auto 16px' }}>
                {currentStep.illustration}

                {/* Animated Circular Countdown Overlay */}
                {isPrepState && (
                  <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    borderRadius: '12px',
                    background: 'rgba(9, 14, 24, 0.85)',
                    backdropFilter: 'blur(3px)',
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                    border: '2px solid #fbbf24',
                    boxShadow: '0 0 20px rgba(251,191,36,0.4)',
                  }}>
                    <div style={{
                      fontSize: '48px',
                      fontWeight: 900,
                      color: '#fbbf24',
                      lineHeight: 1,
                      textShadow: '0 0 16px rgba(251,191,36,0.8)',
                    }}>
                      {prepCountdown}
                    </div>
                    <div style={{ fontSize: '11px', color: '#fff', textTransform: 'uppercase', letterSpacing: '0.08em', marginTop: '6px', fontWeight: 700 }}>
                      Starting in...
                    </div>
                  </div>
                )}
              </div>

              {/* Step Title & Instruction */}
              <div style={{ fontSize: '11px', color: isPrepState ? '#fbbf24' : currentStep.color, fontWeight: 800, textTransform: 'uppercase', letterSpacing: '0.08em', marginBottom: '6px' }}>
                Step {currentStep.number} of 5 — {currentStep.title}
              </div>

              <div style={{ fontSize: '16px', fontWeight: 800, color: '#fff', lineHeight: 1.45, marginBottom: '10px' }}>
                {currentStep.instruction}
              </div>

              <div style={{ fontSize: '12px', color: 'var(--text-muted)', lineHeight: 1.6, background: 'rgba(0,0,0,0.25)', borderRadius: '8px', padding: '10px', textAlign: 'left' }}>
                💡 <strong>Positioning Guide:</strong> {currentStep.prepTip}
              </div>

              {/* User Confirmation Ready Button */}
              {isAwaitingUser && !isPrepState && (
                <div style={{
                  marginTop: '16px',
                  background: 'rgba(0, 229, 255, 0.08)',
                  border: '1px solid rgba(0, 229, 255, 0.3)',
                  borderRadius: '12px',
                  padding: '14px',
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  gap: '10px',
                }}>
                  <div style={{ fontSize: '13px', fontWeight: 700, color: '#fff' }}>
                    Position your hand as shown. Are you ready?
                  </div>
                  <button
                    onClick={handleReadyClick}
                    className="btn-primary"
                    style={{
                      padding: '12px 28px',
                      fontSize: '14px',
                      fontWeight: 800,
                      borderRadius: '10px',
                      boxShadow: '0 0 20px rgba(0, 229, 255, 0.4)',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                    }}
                  >
                    <span>👉</span> Yes, I'm Ready (3s Countdown)
                  </button>
                </div>
              )}

              {/* Step Progress Bar during sampling */}
              {isSampling && (
                <div style={{ marginTop: '16px' }}>
                  <div style={{ height: '8px', background: 'rgba(0,0,0,0.4)', borderRadius: '4px', overflow: 'hidden' }}>
                    <div style={{
                      height: '100%',
                      width: `${calibration.progress_pct}%`,
                      background: 'linear-gradient(90deg, var(--cyan-primary), var(--emerald-valid))',
                      transition: 'width 0.25s linear',
                    }} />
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', color: 'var(--text-dim)', marginTop: '4px' }}>
                    <span style={{ color: 'var(--emerald-valid)', fontWeight: 700 }}>● Active measurement — hold steady</span>
                    <span>{calibration.progress_pct}%</span>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {/* ── CONTROLS & ACTION BUTTONS ── */}
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '12px', marginTop: '16px' }}>
          <button
            onClick={onResetCalibration}
            className="btn-secondary"
            style={{ color: 'var(--crimson-fault)', fontSize: '12px', padding: '8px 14px' }}
          >
            Reset Defaults
          </button>

          <div style={{ display: 'flex', gap: '8px' }}>
            {isRunning && (
              <button onClick={onCancelCalibration} className="btn-secondary" style={{ padding: '8px 16px' }}>
                Cancel
              </button>
            )}
            
            {!isRunning && (
              <button
                onClick={onStartCalibration}
                className="btn-primary"
                style={{ padding: '8px 20px', fontWeight: 700 }}
              >
                {isIdle ? '▶ Start Calibration' : isDone ? '↺ Redo Calibration' : '↺ Try Again'}
              </button>
            )}
          </div>
        </div>

      </div>
    </div>
  );
};
