import { TelemetryPacket, CalibrationProfile, DiagnosticsReport } from '../types';

const getInitialHost = () => {
  if (typeof window !== 'undefined') {
    const params = new URLSearchParams(window.location.search);
    if (params.get('sim') === '1' || params.get('simulator') === '1') {
      return 'http://localhost:8000';
    }
    const customHost = params.get('host');
    if (customHost) return customHost.startsWith('http') ? customHost : `http://${customHost}`;
    const saved = localStorage.getItem('gripguard_api_host');
    if (saved) return saved;
  }
  return 'http://10.249.34.96';
};

let deviceBaseUrl = getInitialHost();

export const setApiHost = (host: string) => {
  deviceBaseUrl = host.startsWith('http') ? host : `http://${host}`;
  if (typeof window !== 'undefined') {
    localStorage.setItem('gripguard_api_host', deviceBaseUrl);
  }
};

export const getApiHost = () => deviceBaseUrl;

export const fetchTelemetry = async (): Promise<TelemetryPacket> => {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 1200);

  try {
    const res = await fetch(`${deviceBaseUrl}/api/telemetry`, { signal: controller.signal });
    clearTimeout(timeoutId);
    if (!res.ok) throw new Error(`HTTP error ${res.status}`);
    return await res.json();
  } catch (err) {
    clearTimeout(timeoutId);
    throw err;
  }
};

export const fetchCalibration = async (): Promise<CalibrationProfile> => {
  const res = await fetch(`${deviceBaseUrl}/api/calibration`);
  if (!res.ok) throw new Error(`HTTP error ${res.status}`);
  return await res.json();
};

export const postCalibrateAction = async (action: 'start' | 'ready' | 'next' | 'cancel' | 'reset') => {
  const res = await fetch(`${deviceBaseUrl}/api/calibrate?action=${action}`, { method: 'POST' });
  return await res.json();
};

export const fetchDiagnostics = async (): Promise<DiagnosticsReport> => {
  const res = await fetch(`${deviceBaseUrl}/api/diagnostics`);
  return await res.json();
};

export const postExerciseProfile = async (profile: 'bicep_curl' | 'wrist_extension' | 'dumbbell_press') => {
  const res = await fetch(`${deviceBaseUrl}/api/exercise?profile=${profile}`, { method: 'POST' });
  return await res.json();
};

export const postResetReps = async () => {
  const res = await fetch(`${deviceBaseUrl}/api/reset_reps`, { method: 'POST' });
  return await res.json();
};
