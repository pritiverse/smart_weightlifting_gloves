#include "Config.h"
#include "SensorAcquisition.h"

SensorAcquisition::SensorAcquisition()
    : _sequenceCounter(0), _lastSampleTimeUs(0) {
    _health.mpuInitialized = false;
    _health.mpuResponding = false;
    _health.mpuAddress = 0;
    _health.lastI2cErrorCode = 0;
    _health.consecutiveI2cFailures = 0;
    _health.totalI2cErrors = 0;
    _health.lastSuccessfulReadMs = 0;
    _health.sampleJitterUs = 0;
    _health.fsrSaturatedHigh = false;
    _health.fsrOpenCircuit = true;
}

uint8_t SensorAcquisition::scanI2C() {
    uint8_t addresses[] = {Config::Hardware::MPU6050_PRIMARY_ADDR, Config::Hardware::MPU6050_ALT_ADDR};
    for (uint8_t addr : addresses) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            return addr;
        }
    }
    return 0;
}

bool SensorAcquisition::init() {
    // 1. Configure FSR Pin on GPIO 34 (ADC1)
    // GPIO 34 is strictly an input pin on ESP32
    pinMode(Config::Hardware::PIN_FSR_ADC, INPUT);
#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(12); // 12-bit resolution: 0 - 4095
    analogSetAttenuation(ADC_11db); // 11dB attenuation: ~0.15V to 3.15V linear range
#endif

    // 2. Initialize I2C Bus on GPIO 21 (SDA) and GPIO 22 (SCL)
    Wire.begin(Config::Hardware::PIN_I2C_SDA, Config::Hardware::PIN_I2C_SCL, Config::Hardware::I2C_CLOCK_SPEED_HZ);
#if defined(ARDUINO_ARCH_ESP32)
    Wire.setTimeOut(25); // 25ms timeout to prevent blocking the acquisition loop
#endif

    // 3. Scan and locate MPU6050 address
    _health.mpuAddress = scanI2C();
    if (_health.mpuAddress == 0) {
        _health.mpuInitialized = false;
        _health.mpuResponding = false;
        _health.lastI2cErrorCode = 2; // Address NACK
        return false;
    }

    // 4. Verify WHO_AM_I register (0x75)
    uint8_t whoAmI = 0;
    if (!readRegisters(_health.mpuAddress, REG_WHO_AM_I, 1, &whoAmI)) {
        _health.mpuInitialized = false;
        return false;
    }

    // MPU6050 standard WHO_AM_I is 0x68 (some variants return 0x70, 0x72, or 0x69)
    if (whoAmI != 0x68 && whoAmI != 0x70 && whoAmI != 0x72 && whoAmI != 0x69) {
        // Unknown device ID, still try to initialize but record warning
    }

    // 5. Wake up MPU6050: Write 0x01 to PWR_MGMT_1 (PLL with X-axis gyro reference)
    if (!writeRegister(_health.mpuAddress, REG_PWR_MGMT_1, 0x01)) return false;
    delay(10); // Allow PLL oscillator to stabilize

    // 6. Set Digital Low Pass Filter (DLPF) to 44 Hz Accel / 42 Hz Gyro (DLPF_CFG = 3)
    if (!writeRegister(_health.mpuAddress, REG_CONFIG, 0x03)) return false;

    // 7. Set Sample Rate Divider to 1 -> Sample Rate = 1 kHz / (1 + 1) = 500 Hz internal
    if (!writeRegister(_health.mpuAddress, REG_SMPLRT_DIV, 0x01)) return false;

    // 8. Configure Gyroscope Full-Scale Range (+/- 1000 deg/s: FS_SEL = 2 -> 0x10)
    if (!writeRegister(_health.mpuAddress, REG_GYRO_CONFIG, 0x10)) return false;

    // 9. Configure Accelerometer Full-Scale Range (+/- 4g: AFS_SEL = 1 -> 0x08)
    if (!writeRegister(_health.mpuAddress, REG_ACCEL_CONFIG, 0x08)) return false;

    _health.mpuInitialized = true;
    _health.mpuResponding = true;
    _health.lastSuccessfulReadMs = millis();
    _lastSampleTimeUs = micros();

    return true;
}

bool SensorAcquisition::writeRegister(uint8_t devAddr, uint8_t regAddr, uint8_t data) {
    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    Wire.write(data);
    uint8_t err = Wire.endTransmission();
    _health.lastI2cErrorCode = err;
    if (err != 0) {
        _health.totalI2cErrors++;
        _health.consecutiveI2cFailures++;
        return false;
    }
    return true;
}

bool SensorAcquisition::readRegisters(uint8_t devAddr, uint8_t regAddr, uint8_t count, uint8_t* dest) {
    Wire.beginTransmission(devAddr);
    Wire.write(regAddr);
    uint8_t err = Wire.endTransmission(false); // Repeated start
    _health.lastI2cErrorCode = err;
    if (err != 0) {
        _health.totalI2cErrors++;
        _health.consecutiveI2cFailures++;
        return false;
    }

    uint8_t bytesReceived = Wire.requestFrom((int)devAddr, (int)count, (int)true);
    if (bytesReceived != count) {
        _health.totalI2cErrors++;
        _health.consecutiveI2cFailures++;
        _health.lastI2cErrorCode = 4; // Data read mismatch
        return false;
    }

    for (uint8_t i = 0; i < count; i++) {
        dest[i] = Wire.read();
    }
    return true;
}

uint16_t SensorAcquisition::readFsrOversampled() {
    uint16_t samples[Config::Timing::FSR_OVERSAMPLE_COUNT];

    // Rapid burst acquisition with 50 us spacing
    for (uint8_t i = 0; i < Config::Timing::FSR_OVERSAMPLE_COUNT; i++) {
        samples[i] = analogRead(Config::Hardware::PIN_FSR_ADC);
        if (i < Config::Timing::FSR_OVERSAMPLE_COUNT - 1) {
            delayMicroseconds(Config::Timing::FSR_OVERSAMPLE_DELAY_US);
        }
    }

    // Insertion sort to extract trimmed mean
    for (uint8_t i = 1; i < Config::Timing::FSR_OVERSAMPLE_COUNT; i++) {
        uint16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }

    // Discard 2 lowest and 2 highest outliers, average middle 4 samples
    uint32_t sum = 0;
    for (uint8_t i = 2; i < Config::Timing::FSR_OVERSAMPLE_COUNT - 2; i++) {
        sum += samples[i];
    }
    return (uint16_t)(sum / (Config::Timing::FSR_OVERSAMPLE_COUNT - 4));
}

bool SensorAcquisition::readSensors(RawSensorData& outData) {
    uint32_t nowUs = micros();
    uint32_t dt = nowUs - _lastSampleTimeUs;
    _lastSampleTimeUs = nowUs;

    outData.sequenceId = ++_sequenceCounter;
    outData.timestampUs = nowUs;
    outData.timestampMs = millis();
    outData.dtUs = dt;
    _health.sampleJitterUs = (int32_t)dt - (int32_t)Config::Timing::SAMPLE_INTERVAL_US;

    // --- 1. MPU6050 Burst Read (14 Bytes) ---
    bool mpuSuccess = false;
    if (_health.mpuInitialized) {
        uint8_t buffer[14];
        if (readRegisters(_health.mpuAddress, REG_ACCEL_XOUT_H, 14, buffer)) {
            outData.rawAccelX = (int16_t)((buffer[0] << 8) | buffer[1]);
            outData.rawAccelY = (int16_t)((buffer[2] << 8) | buffer[3]);
            outData.rawAccelZ = (int16_t)((buffer[4] << 8) | buffer[5]);

            int16_t rawTemp = (int16_t)((buffer[6] << 8) | buffer[7]);

            outData.rawGyroX  = (int16_t)((buffer[8] << 8) | buffer[9]);
            outData.rawGyroY  = (int16_t)((buffer[10] << 8) | buffer[11]);
            outData.rawGyroZ  = (int16_t)((buffer[12] << 8) | buffer[13]);

            // Convert to physical units (m/s^2, deg/s, deg C)
            outData.accelX_mps2 = outData.rawAccelX * ACCEL_SCALE_4G;
            outData.accelY_mps2 = outData.rawAccelY * ACCEL_SCALE_4G;
            outData.accelZ_mps2 = outData.rawAccelZ * ACCEL_SCALE_4G;

            outData.gyroX_dps = outData.rawGyroX * GYRO_SCALE_1000DPS;
            outData.gyroY_dps = outData.rawGyroY * GYRO_SCALE_1000DPS;
            outData.gyroZ_dps = outData.rawGyroZ * GYRO_SCALE_1000DPS;

            outData.temperatureC = (rawTemp / 340.0f) + 36.53f;

            _health.mpuResponding = true;
            _health.consecutiveI2cFailures = 0;
            _health.lastSuccessfulReadMs = outData.timestampMs;
            mpuSuccess = true;
        } else {
            _health.mpuResponding = false;
        }
    }

    if (!mpuSuccess) {
        // Safe fallback in case of I2C blackout
        outData.rawAccelX = 0;
        outData.rawAccelY = 0;
        outData.rawAccelZ = 0;
        outData.accelX_mps2 = 0.0f;
        outData.accelY_mps2 = 0.0f;
        outData.accelZ_mps2 = 0.0f;
        outData.rawGyroX = 0;
        outData.rawGyroY = 0;
        outData.rawGyroZ = 0;
        outData.gyroX_dps = 0.0f;
        outData.gyroY_dps = 0.0f;
        outData.gyroZ_dps = 0.0f;
        outData.temperatureC = 0.0f;
    }

    // --- 2. FSR Oversampled ADC Read ---
    outData.fsrRawADC = readFsrOversampled();
    
    // Voltage calculation on 3.3V rail: V_adc = (ADC / 4095.0) * 3.30V
    outData.fsrVoltage = (outData.fsrRawADC / (float)Config::Hardware::ADC_MAX_COUNT) * Config::Hardware::VCC_VOLTS;

    // FSR Resistance calculation from voltage divider:
    // V_adc = Vcc * (R_div / (R_fsr + R_div))
    // -> R_fsr = R_div * (Vcc / V_adc - 1)
    if (outData.fsrVoltage > 0.03f && outData.fsrVoltage < (Config::Hardware::VCC_VOLTS - 0.03f)) {
        outData.fsrEstimatedOhms = Config::Hardware::R_DIVIDER_OHMS * 
            ((Config::Hardware::VCC_VOLTS / outData.fsrVoltage) - 1.0f);
        _health.fsrOpenCircuit = false;
        _health.fsrSaturatedHigh = false;
    } else if (outData.fsrVoltage >= (Config::Hardware::VCC_VOLTS - 0.03f)) {
        outData.fsrEstimatedOhms = 0.0f; // Near dead-short / saturation
        _health.fsrSaturatedHigh = true;
        _health.fsrOpenCircuit = false;
    } else {
        outData.fsrEstimatedOhms = 10000000.0f; // >10 MOhms / open circuit
        _health.fsrOpenCircuit = true;
        _health.fsrSaturatedHigh = false;
    }

    return mpuSuccess;
}
