export interface OrientationData {
  pitch_deg: number;
  roll_deg: number;
  is_stationary: boolean;
  stationary_ms: number;
}

export interface ImuPhysicalData {
  accel_x: number;
  accel_y: number;
  accel_z: number;
  gyro_x: number;
  gyro_y: number;
  gyro_z: number;
  temp_c: number;
}

export interface GripData {
  raw_adc: number;
  filtered_adc: number;
  voltage: number;
  normalized_pct: number;
  schmitt_armed?: boolean;
}

export interface RepetitionData {
  state: 'IDLE' | 'READY' | 'CONCENTRIC' | 'TRANSITION' | 'ECCENTRIC' | 'EVALUATING' | 'VALID_REP' | 'REJECTED_REP';
  exercise: string;
  valid_count: number;
  rejected_count: number;
  acceptance_ratio_pct: number;
  current_excursion_deg: number;
  last_rep_duration_ms: number;
  last_rep_rom_deg: number;
  last_rep_grip_pct: number;
  last_rejection_reason: string;
  coupling_rho?: number;
  confidence_pct?: number;
  is_fatigued?: boolean;
  fatigue_index_pct?: number;
  adaptive_min_rom_deg?: number;
  pending_candidates?: number;
  is_set_armed?: boolean;
}

export interface HealthData {
  overall: 'HEALTHY' | 'DEGRADED' | 'SUSPECTED_FAULT' | 'SENSOR_UNAVAILABLE' | 'RECOVERING' | 'CALIBRATION_REQUIRED';
  mpu: string;
  fsr: string;
  confidence_pct: number;
  active_fault: string;
  message: string;
  rep_counting_gated: boolean;
}

export interface CalibrationSummary {
  is_calibrated: boolean;
  step: string;
  progress_pct: number;
  failure_reason?: string;
}

export interface TelemetryPacket {
  schema_version: string;
  device_id: string;
  sequence_id: number;
  uptime_ms: number;
  sample_dt_us: number;
  orientation: OrientationData;
  imu_physical: ImuPhysicalData;
  grip: GripData;
  repetition: RepetitionData;
  health: HealthData;
  calibration: CalibrationSummary;
}

export interface CalibrationProfile {
  is_valid: boolean;
  schema_version: number;
  neutral_pitch_deg: number;
  neutral_roll_deg: number;
  posture_stability_std: number;
  fsr_relaxed_mean_adc: number;
  fsr_relaxed_std: number;
  fsr_firm_mean_adc: number;
  fsr_firm_std: number;
  grip_separation_ratio: number;
  grip_eligibility_adc: number;
  posture_tolerance_deg: number;
  crc32: number;
}

export interface DiagnosticsReport {
  overall_health: string;
  mpu_health: string;
  fsr_health: string;
  active_fault: string;
  fault_message: string;
  confidence_pct: number;
  rep_counting_permitted: boolean;
  gyro_bias_mag_dps: number;
  fsr_baseline_delta_adc: number;
  consecutive_i2c_errors: number;
  recovery_elapsed_ms: number;
}

export interface HistoricalRepItem {
  id: number;
  repNumber: number;
  timestamp: string;
  exercise: string;
  isValid: boolean;
  durationMs: number;
  peakRomDeg: number;
  avgGripPct: number;
  couplingRho?: number;
  confidencePct?: number;
  isFatigued?: boolean;
  rejectionReason: string;
}
