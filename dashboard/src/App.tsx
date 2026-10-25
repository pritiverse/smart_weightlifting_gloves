import { useState, useEffect, useRef } from 'react';
import { Header } from './components/Header';
import { OverviewCards } from './components/OverviewCards';
import { MovementTrace } from './components/MovementTrace';
import { SensorPanel } from './components/SensorPanel';
import { ReliabilityCentre } from './components/ReliabilityCentre';
import { SessionReport } from './components/SessionReport';
import { CalibrationModal } from './components/CalibrationModal';
import {
  fetchTelemetry,
  fetchCalibration,
  postCalibrateAction,
  postExerciseProfile,
  postResetReps,
  setApiHost,
  getApiHost,
} from './services/api';
import { TelemetryPacket, CalibrationProfile, HistoricalRepItem } from './types';

export function App() {
  const [telemetry, setTelemetry] = useState<TelemetryPacket | null>(null);
  const [calibProfile, setCalibProfile] = useState<CalibrationProfile | null>(null);
  const [historyPitch, setHistoryPitch] = useState<number[]>([]);
  const [repHistory, setRepHistory] = useState<HistoricalRepItem[]>([]);
  const [isConnected, setIsConnected] = useState(false);
  const [latencyMs, setLatencyMs] = useState(0);
  const [isCalibModalOpen, setIsCalibModalOpen] = useState(false);
  const [apiHost, setHostState] = useState(getApiHost());
  const [sessionSeconds, setSessionSeconds] = useState(0);

  const prevValidCountRef = useRef(0);
  const prevRejCountRef = useRef(0);

  // 1. Session Timer
  useEffect(() => {
    const timer = setInterval(() => {
      setSessionSeconds((prev) => prev + 1);
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // 2. High-Speed 10 Hz Real-Time Telemetry Polling Loop
  useEffect(() => {
    let isSubscribed = true;

    const poll = async () => {
      const startTime = performance.now();
      try {
        const data = await fetchTelemetry();
        if (!isSubscribed) return;

        const roundTrip = Math.round(performance.now() - startTime);
        setLatencyMs(roundTrip);
        setIsConnected(true);
        setTelemetry(data);

        // Append to real-time trajectory buffer (max 80 samples)
        setHistoryPitch((prev) => {
          const next = [...prev, data.orientation.pitch_deg];
          return next.length > 80 ? next.slice(next.length - 80) : next;
        });

        // Detect newly completed repetition
        const newValid = data.repetition.valid_count > prevValidCountRef.current;
        const newRej = data.repetition.rejected_count > prevRejCountRef.current;

        if (newValid || newRej) {
          prevValidCountRef.current = data.repetition.valid_count;
          prevRejCountRef.current = data.repetition.rejected_count;

          const totalRepNum = data.repetition.valid_count + data.repetition.rejected_count;
          const newItem: HistoricalRepItem = {
            id: Date.now(),
            repNumber: totalRepNum,
            timestamp: new Date().toLocaleTimeString(),
            exercise: data.repetition.exercise,
            isValid: newValid,
            durationMs: data.repetition.last_rep_duration_ms,
            peakRomDeg: data.repetition.last_rep_rom_deg,
            avgGripPct: data.repetition.last_rep_grip_pct,
            rejectionReason: data.repetition.last_rejection_reason,
          };
          setRepHistory((prev) => [newItem, ...prev]);
        }
      } catch {
        if (isSubscribed) {
          setIsConnected(false);
        }
      }
    };

    poll();
    const interval = setInterval(poll, 100);

    return () => {
      isSubscribed = false;
      clearInterval(interval);
    };
  }, []);

  // 3. Load initial calibration profile
  useEffect(() => {
    fetchCalibration()
      .then(setCalibProfile)
      .catch(() => {});
  }, []);

  const handleUpdateHost = (host: string) => {
    setApiHost(host);
    setHostState(getApiHost());
  };

  const handleResetReps = async () => {
    await postResetReps();
    prevValidCountRef.current = 0;
    prevRejCountRef.current = 0;
    setRepHistory([]);
    setSessionSeconds(0);
  };

  const handleSelectExercise = async (profile: 'bicep_curl' | 'wrist_extension' | 'dumbbell_press') => {
    await postExerciseProfile(profile);
  };

  const handleStartCalibration = async () => {
    await postCalibrateAction('start');
  };

  const handleCancelCalibration = async () => {
    await postCalibrateAction('cancel');
  };

  const handleResetCalibration = async () => {
    await postCalibrateAction('reset');
    const updated = await fetchCalibration();
    setCalibProfile(updated);
  };

  const handleExportCSV = () => {
    if (repHistory.length === 0) return;
    const headers = ['Rep #', 'Timestamp', 'Exercise', 'Status', 'Duration (ms)', 'Peak ROM (deg)', 'Avg Grip (%)', 'Rejection Reason'];
    const rows = repHistory.map((r) => [
      r.repNumber,
      `"${r.timestamp}"`,
      `"${r.exercise}"`,
      r.isValid ? 'VALID' : 'REJECTED',
      r.durationMs,
      r.peakRomDeg,
      r.avgGripPct,
      `"${r.rejectionReason}"`,
    ]);
    const csvContent = [headers.join(','), ...rows.map((e) => e.join(','))].join('\n');
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.setAttribute('href', url);
    link.setAttribute('download', `gripguard_workout_session_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const handleExportJSON = () => {
    const data = {
      device_id: telemetry?.device_id || 'GRIPGUARD_ESP32',
      session_exported_at: new Date().toISOString(),
      session_duration_seconds: sessionSeconds,
      summary: telemetry?.repetition,
      calibration_profile: calibProfile,
      repetitions_log: repHistory,
    };
    const jsonStr = JSON.stringify(data, null, 2);
    const blob = new Blob([jsonStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.setAttribute('href', url);
    link.setAttribute('download', `gripguard_session_${Date.now()}.json`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div className="dashboard-container">
      <Header
        isConnected={isConnected}
        latencyMs={latencyMs}
        onOpenCalibration={() => setIsCalibModalOpen(true)}
        apiHost={apiHost}
        onUpdateHost={handleUpdateHost}
      />

      {telemetry ? (
        <div className="dashboard-grid">
          {/* Hero Counter Cards */}
          <OverviewCards
            repetition={telemetry.repetition}
            sessionElapsedSeconds={sessionSeconds}
            onResetReps={handleResetReps}
            onSelectExercise={handleSelectExercise}
          />

          {/* Real-time Movement Curve */}
          <MovementTrace repetition={telemetry.repetition} historyPitch={historyPitch} />

          {/* Sensor Panel */}
          <SensorPanel grip={telemetry.grip} orientation={telemetry.orientation} imu={telemetry.imu_physical} />

          {/* Sensor Reliability Centre */}
          <ReliabilityCentre health={telemetry.health} />

          {/* Session Rep Log & Export */}
          <SessionReport history={repHistory} onExportCSV={handleExportCSV} onExportJSON={handleExportJSON} />
        </div>
      ) : (
        <div style={{ textAlign: 'center', padding: '100px 20px' }}>
          <div className="brand-icon" style={{ margin: '0 auto 20px', width: '56px', height: '56px', animation: 'pulse 2s infinite' }}>
            <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="#040814" strokeWidth="2.5">
              <path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6" />
            </svg>
          </div>
          <h2 style={{ fontSize: '20px', fontWeight: 700, marginBottom: '8px' }}>
            Waiting for Real-Time GRIPGUARD Hardware...
          </h2>
          <p style={{ color: 'var(--text-muted)', fontSize: '14px', maxWidth: '520px', margin: '0 auto 20px', lineHeight: 1.6 }}>
            Streaming endpoint: <code>{apiHost}/api/telemetry</code>.<br />
            Please connect your computer to the glove Wi-Fi: <strong>GripGuard-AP</strong> (password: <code>smartglove123</code>).
          </p>
          <div style={{ display: 'inline-flex', alignItems: 'center', gap: '8px', color: 'var(--cyan-primary)', fontSize: '13px' }}>
            <span className="status-dot pulse" style={{ width: '8px', height: '8px', background: 'var(--cyan-primary)' }} />
            Listening for live 10 Hz physical telemetry stream...
          </div>
        </div>
      )}

      {/* 5-Step Guided Calibration Modal */}
      <CalibrationModal
        isOpen={isCalibModalOpen}
        onClose={() => setIsCalibModalOpen(false)}
        calibration={telemetry?.calibration || { is_calibrated: false, step: 'IDLE', progress_pct: 0 }}
        profile={calibProfile}
        onStartCalibration={handleStartCalibration}
        onCancelCalibration={handleCancelCalibration}
        onResetCalibration={handleResetCalibration}
      />
    </div>
  );
}
