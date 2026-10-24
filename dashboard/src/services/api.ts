import { TelemetryPacket, CalibrationProfile, DiagnosticsReport } from '../types';

let deviceBaseUrl = 'http://192.168.4.1';
let isSimulationMode = false;
let simSeq = 0;
let simValidReps = 0;
let simRejReps = 0;
let simPhaseAngle = 0;

export const setApiHost = (host: string) => {
  deviceBaseUrl = host.startsWith('http') ? host : `http://${host}`;
};

export const getApiHost = () => deviceBaseUrl;

export const setSimulationMode = (enable: boolean) => {
  isSimulationMode = enable;
};

export const getSimulationMode = () => isSimulationMode;

// Generates scientifically realistic synthetic trajectory for offline dashboard demonstration
const generateSyntheticTelemetry = (): TelemetryPacket => {
  simSeq++;
  simPhaseAngle += 0.05;
  if (simPhaseAngle > Math.PI * 2) {
    simPhaseAngle -= Math.PI * 2;
    if (Math.random() > 0.15) {
      simValidReps++;
    } else {
      simRejReps++;
    }
  }

  const pitch = Math.sin(simPhaseAngle) * 35 + 10;
  const isConcentric = Math.cos(simPhaseAngle) > 0.2;
  const grip = isConcentric ? 72 + Math.sin(simSeq * 0.2) * 8 : 15;
  const state = isConcentric ? 'CONCENTRIC' : 'ECCENTRIC';

  const total = simValidReps + simRejReps;
  const ratio = total > 0 ? (simValidReps / total) * 100 : 100;

  return {
    schema_version: '1.0',
    device_id: 'GRIPGUARD_SIMULATOR',
    sequence_id: simSeq,
    uptime_ms: simSeq * 100,
    sample_dt_us: 20015,
    orientation: {
      pitch_deg: Math.round(pitch * 10) / 10,
      roll_deg: Math.round(Math.sin(simPhaseAngle * 0.5) * 4 * 10) / 10,
      is_stationary: false,
      stationary_ms: 0,
    },
    imu_physical: {
      accel_x: Math.round(Math.sin(simPhaseAngle) * 4.5 * 100) / 100,
      accel_y: Math.round((9.81 + Math.cos(simPhaseAngle) * 2.0) * 100) / 100,
      accel_z: 0.35,
      gyro_x: Math.round(Math.cos(simPhaseAngle) * 45 * 10) / 10,
      gyro_y: Math.round(-Math.sin(simPhaseAngle) * 12 * 10) / 10,
      gyro_z: 0.5,
      temp_c: 28.5,
    },
    grip: {
      raw_adc: Math.round(grip * 28 + 350),
      filtered_adc: Math.round(grip * 28 + 350),
      voltage: Math.round(((grip * 28 + 350) / 4095) * 3.3 * 100) / 100,
      normalized_pct: Math.round(grip),
    },
    repetition: {
      state: state as any,
      exercise: 'Bicep Curl',
      valid_count: simValidReps,
      rejected_count: simRejReps,
      acceptance_ratio_pct: Math.round(ratio * 10) / 10,
      current_excursion_deg: Math.round(Math.abs(pitch) * 10) / 10,
      last_rep_duration_ms: 2150,
      last_rep_rom_deg: 58.4,
      last_rep_grip_pct: 76,
      last_rejection_reason: simRejReps > 0 ? 'INSUFFICIENT_RANGE_OF_MOTION' : 'NONE (VALID)',
    },
    health: {
      overall: 'HEALTHY',
      mpu: 'HEALTHY',
      fsr: 'HEALTHY',
      confidence_pct: 100,
      active_fault: 'NONE',
      message: 'All sensors nominal (Live Synthetic Stream)',
      rep_counting_gated: false,
    },
    calibration: {
      is_calibrated: true,
      step: 'IDLE',
      progress_pct: 0,
    },
  };
};

export const fetchTelemetry = async (): Promise<TelemetryPacket> => {
  if (isSimulationMode) {
    return generateSyntheticTelemetry();
  }

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
  if (isSimulationMode) {
    return {
      is_valid: true,
      schema_version: 1,
      neutral_pitch_deg: 0.5,
      neutral_roll_deg: -1.1,
      posture_stability_std: 1.15,
      fsr_relaxed_mean_adc: 350,
      fsr_relaxed_std: 22.4,
      fsr_firm_mean_adc: 2420,
      fsr_firm_std: 48.6,
      grip_separation_ratio: 29.15,
      grip_eligibility_adc: 1178,
      posture_tolerance_deg: 15.0,
      crc32: 394857102,
    };
  }

  const res = await fetch(`${deviceBaseUrl}/api/calibration`);
  if (!res.ok) throw new Error(`HTTP error ${res.status}`);
  return await res.json();
};

export const postCalibrateAction = async (action: 'start' | 'cancel' | 'reset') => {
  if (isSimulationMode) return { status: 'ok', action };
  const res = await fetch(`${deviceBaseUrl}/api/calibrate?action=${action}`, { method: 'POST' });
  return await res.json();
};

export const fetchDiagnostics = async (): Promise<DiagnosticsReport> => {
  if (isSimulationMode) {
    return {
      overall_health: 'HEALTHY',
      mpu_health: 'HEALTHY',
      fsr_health: 'HEALTHY',
      active_fault: 'NONE',
      fault_message: 'All sensors nominal (Simulation Active)',
      confidence_pct: 100,
      rep_counting_permitted: true,
      gyro_bias_mag_dps: 0.25,
      fsr_baseline_delta_adc: 8.5,
      consecutive_i2c_errors: 0,
      recovery_elapsed_ms: 0,
    };
  }

  const res = await fetch(`${deviceBaseUrl}/api/diagnostics`);
  return await res.json();
};

export const postExerciseProfile = async (profile: 'bicep_curl' | 'wrist_extension' | 'dumbbell_press') => {
  if (isSimulationMode) return { status: 'ok', exercise: profile };
  const res = await fetch(`${deviceBaseUrl}/api/exercise?profile=${profile}`, { method: 'POST' });
  return await res.json();
};

export const postResetReps = async () => {
  if (isSimulationMode) {
    simValidReps = 0;
    simRejReps = 0;
    return { status: 'repetition_counters_reset' };
  }
  const res = await fetch(`${deviceBaseUrl}/api/reset_reps`, { method: 'POST' });
  return await res.json();
};
