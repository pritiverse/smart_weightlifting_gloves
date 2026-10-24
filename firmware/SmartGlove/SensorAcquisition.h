#ifndef SENSOR_ACQUISITION_H
#define SENSOR_ACQUISITION_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

/**
 * Data container for single synchronized sensor acquisition cycle
 */
struct RawSensorData {
    // Monotonic sample counters and timestamps
    uint32_t sequenceId;
    uint32_t timestampUs;
    uint32_t timestampMs;
    uint32_t dtUs;

    // MPU6050 Accelerometer (m/s^2) and raw integers
    int16_t rawAccelX;
    int16_t rawAccelY;
    int16_t rawAccelZ;
    float accelX_mps2;
    float accelY_mps2;
    float accelZ_mps2;

    // MPU6050 Gyroscope (deg/s) and raw integers
    int16_t rawGyroX;
    int16_t rawGyroY;
    int16_t rawGyroZ;
    float gyroX_dps;
    float gyroY_dps;
    float gyroZ_dps;

    // MPU6050 Die Temperature (deg C)
    float temperatureC;

    // FSR Analog Measurements on GPIO 34
    uint16_t fsrRawADC;       // 12-bit ADC (0 - 4095)
    float fsrVoltage;         // Volts (0.00 - 3.30V)
    float fsrEstimatedOhms;   // Calculated FSR resistance
};

/**
 * Low-level hardware health diagnostics for acquisition stage
 */
struct AcquisitionHealth {
    bool mpuInitialized;
    bool mpuResponding;
    uint8_t mpuAddress;       // 0x68 or 0x69
    uint8_t lastI2cErrorCode; // 0=OK, 2=Addr NACK, 3=Data NACK, 4=Other, 5=Timeout
    uint32_t consecutiveI2cFailures;
    uint32_t totalI2cErrors;
    uint32_t lastSuccessfulReadMs;
    int32_t sampleJitterUs;   // Difference between actual dt and target 20000 us
    bool fsrSaturatedHigh;    // ADC pinned near 4095
    bool fsrOpenCircuit;      // ADC pinned at 0
};

class SensorAcquisition {
public:
    SensorAcquisition();

    /**
     * Initialize I2C bus on GPIO 21/22, verify MPU6050 identity, and configure ADC on GPIO 34.
     * Returns true if both sensors are operational.
     */
    bool init();

    /**
     * Execute synchronized acquisition cycle (MPU6050 via I2C, FSR via 8x oversampled ADC).
     * Populates outData and updates acquisition health telemetry.
     */
    bool readSensors(RawSensorData& outData);

    /**
     * Retrieve current acquisition health statistics.
     */
    const AcquisitionHealth& getHealth() const { return _health; }

    /**
     * Scan I2C bus and return detected address (0x68, 0x69, or 0 if missing).
     */
    uint8_t scanI2C();

private:
    AcquisitionHealth _health;
    uint32_t _sequenceCounter;
    uint32_t _lastSampleTimeUs;

    // MPU6050 Register Addresses
    static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
    static constexpr uint8_t REG_CONFIG       = 0x1A;
    static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
    static constexpr uint8_t REG_WHO_AM_I     = 0x75;

    // Conversion factors for +/-4g and +/-1000 deg/s
    // +/- 4g range -> 8192 LSB/g. 1g = 9.80665 m/s^2 -> factor = 9.80665 / 8192.0
    static constexpr float ACCEL_SCALE_4G     = 9.80665f / 8192.0f;
    // +/- 1000 deg/s range -> 32.8 LSB/(deg/s) -> factor = 1.0 / 32.8
    static constexpr float GYRO_SCALE_1000DPS = 1.0f / 32.8f;

    bool writeRegister(uint8_t devAddr, uint8_t regAddr, uint8_t data);
    bool readRegisters(uint8_t devAddr, uint8_t regAddr, uint8_t count, uint8_t* dest);
    uint16_t readFsrOversampled();
};

#endif // SENSOR_ACQUISITION_H
