import React from 'react';
import { GripData, OrientationData, ImuPhysicalData } from '../types';

interface SensorPanelProps {
  grip: GripData;
  orientation: OrientationData;
  imu: ImuPhysicalData;
}

export const SensorPanel: React.FC<SensorPanelProps> = ({ grip, orientation, imu }) => {
  return (
    <div className="dashboard-card grid-col-5">
      <div className="card-header">
        <h2 className="card-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--cyan-primary)" strokeWidth="2.5">
            <path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z" />
          </svg>
          Live Sensor Acquisition (GPIO 34 & I2C)
        </h2>
        <span
          style={{
            fontSize: '11px',
            fontWeight: 700,
            padding: '3px 8px',
            borderRadius: '4px',
            background: orientation.is_stationary ? 'rgba(0, 230, 118, 0.15)' : 'rgba(0, 229, 255, 0.15)',
            color: orientation.is_stationary ? 'var(--emerald-valid)' : 'var(--cyan-primary)',
          }}
        >
          {orientation.is_stationary ? 'STATIONARY' : 'ACTIVE DYNAMICS'}
        </span>
      </div>

      {/* FSR Grip Pressure Bar */}
      <div className="grip-bar-container">
        <div className="grip-bar-header">
          <span style={{ color: 'var(--text-muted)' }}>FSR Relative Grip Pressure</span>
          <span style={{ color: 'var(--cyan-primary)', fontFamily: 'var(--font-mono)' }}>
            {grip.normalized_pct}% ({grip.voltage}V / {grip.raw_adc} ADC)
          </span>
        </div>
        <div className="grip-progress-bg">
          <div className="grip-eligibility-marker" style={{ left: '35%' }} />
          <div className="grip-progress-fill" style={{ width: `${Math.min(100, Math.max(0, grip.normalized_pct))}%` }} />
        </div>
      </div>

      {/* 2-Column Sensor Readouts */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '16px' }}>
        {/* Orientation Tilt Gauge */}
        <div style={{ background: 'var(--bg-surface)', padding: '12px', borderRadius: 'var(--radius-md)' }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase' }}>
            Estimated Wrist Tilt
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '6px' }}>
            <div>
              <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>Pitch: </span>
              <strong style={{ fontFamily: 'var(--font-mono)', color: '#fff' }}>{orientation.pitch_deg}°</strong>
            </div>
            <div>
              <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>Roll: </span>
              <strong style={{ fontFamily: 'var(--font-mono)', color: '#fff' }}>{orientation.roll_deg}°</strong>
            </div>
          </div>
        </div>

        {/* Die Temperature & Sampling */}
        <div style={{ background: 'var(--bg-surface)', padding: '12px', borderRadius: 'var(--radius-md)' }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase' }}>
            Hardware Telemetry
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '6px' }}>
            <div>
              <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>Rate: </span>
              <strong style={{ fontFamily: 'var(--font-mono)', color: 'var(--cyan-primary)' }}>50 Hz</strong>
            </div>
            <div>
              <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>Temp: </span>
              <strong style={{ fontFamily: 'var(--font-mono)', color: '#fff' }}>{imu.temp_c}°C</strong>
            </div>
          </div>
        </div>
      </div>

      {/* Raw IMU Accel & Gyro Table */}
      <div style={{ marginTop: '14px', background: 'var(--bg-surface)', padding: '10px 14px', borderRadius: 'var(--radius-md)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11px', color: 'var(--text-dim)', marginBottom: '4px' }}>
          <span>AXIS</span>
          <span>ACCELEROMETER (m/s²)</span>
          <span>GYROSCOPE (deg/s)</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', fontFamily: 'var(--font-mono)' }}>
          <span style={{ color: 'var(--text-muted)' }}>X / Y / Z</span>
          <span style={{ color: '#fff' }}>
            {imu.accel_x.toFixed(1)} / {imu.accel_y.toFixed(1)} / {imu.accel_z.toFixed(1)}
          </span>
          <span style={{ color: 'var(--cyan-primary)' }}>
            {imu.gyro_x.toFixed(1)} / {imu.gyro_y.toFixed(1)} / {imu.gyro_z.toFixed(1)}
          </span>
        </div>
      </div>
    </div>
  );
};
