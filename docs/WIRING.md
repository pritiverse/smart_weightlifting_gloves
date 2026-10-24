# GRIPGUARD — Hardware Wiring & Electrical Guide

This guide details the exact physical wiring, circuit theory, and electrical characteristics for the Smart Weightlifting Glove prototype.

---

## 1. Strict Hardware Pinout Table

| Sensor / Component | Pin / Terminal | ESP32 Connection | Function | Electrical Limits |
|---|---|---|---|---|
| **MPU6050** | VCC | ESP32 `3V3` | Power Supply | 3.0V – 3.6V DC (Do NOT connect to 5V) |
| **MPU6050** | GND | ESP32 `GND` | Ground Reference | Common ground with ESP32 |
| **MPU6050** | SDA | ESP32 `GPIO 21` | I2C Data Line | 3.3V logic; internal/breakout pull-ups to 3.3V |
| **MPU6050** | SCL | ESP32 `GPIO 22` | I2C Clock Line | 3.3V logic; 400 kHz Fast-Mode |
| **MPU6050** | AD0 | ESP32 `GND` (or float) | I2C Address Select | Low = `0x68`, High (3V3) = `0x69` |
| **FSR Sensor** | Terminal 1 | ESP32 `3V3` | Voltage Divider High | Connect to 3.3V power rail |
| **FSR Sensor** | Terminal 2 | ESP32 `GPIO 34` | Sensor Analog Out | Voltage divider midpoint node |
| **10 kΩ Resistor** | Terminal 1 | ESP32 `GPIO 34` | Pull-Down Input | Connects to same node as FSR Terminal 2 |
| **10 kΩ Resistor** | Terminal 2 | ESP32 `GND` | Pull-Down Return | Connect to ESP32 ground |

---

## 2. Circuit Schematic Diagram

```
                 +3.3V Rail (ESP32 Pin 3V3)
                       |
                       +-------------------+
                       |                   |
                     [VCC]                 |
                 +-----------+             |
                 |  MPU6050  |             | FSR Terminal 1
                 |  6-Axis   |             | (Force Sensitive Resistor)
                 |    IMU    |             |
                 +-----------+             |
                   |   |   |               [ R_FSR ]
                 [GND][SDA][SCL]           |
                   |   |   |               | FSR Terminal 2
                   |   |   |               |
                   |   |   +-------------->| ESP32 GPIO 22 (SCL)
                   |   +------------------>| ESP32 GPIO 21 (SDA)
                   |                       |
                   |                       +---------> ESP32 GPIO 34 (ADC1_CH6)
                   |                       |
                   |                       | Terminal 1
                   |                   [ 10 kΩ ] Fixed Pull-Down Resistor
                   |                       | Terminal 2
                   |                       |
                   +-----------------------+---------> ESP32 GND
```

---

## 3. Electrical Characteristics & Physics

### 3.1 FSR Voltage Divider Transfer Function
The FSR changes resistance inversely with applied mechanical force:
- **Unloaded (Relaxed):** $R_{FSR} > 1\text{ M}\Omega$
- **Light Grip:** $R_{FSR} \approx 30\text{ k}\Omega - 100\text{ k}\Omega$
- **Firm Exercise Grip:** $R_{FSR} \approx 1\text{ k}\Omega - 5\text{ k}\Omega$
- **Extreme Squeeze:** $R_{FSR} < 500\Omega$

The voltage presented to ESP32 `GPIO 34` is governed by:
$$V_{ADC} = V_{CC} \cdot \left(\frac{R_{divider}}{R_{FSR} + R_{divider}}\right) = 3.3\text{V} \cdot \left(\frac{10\text{ k}\Omega}{R_{FSR} + 10\text{ k}\Omega}\right)$$

#### Operating Regimes:
1. **Unloaded State:**
   $$V_{ADC} \approx 3.3\text{V} \cdot \left(\frac{10\text{ k}\Omega}{1000\text{ k}\Omega + 10\text{ k}\Omega}\right) \approx 0.032\text{V} \implies \text{ADC} \approx 0 - 50\text{ counts}$$
2. **Firm Weightlifting Grip:**
   $$V_{ADC} \approx 3.3\text{V} \cdot \left(\frac{10\text{ k}\Omega}{2\text{ k}\Omega + 10\text{ k}\Omega}\right) \approx 2.75\text{V} \implies \text{ADC} \approx 3410\text{ counts}$$

### 3.2 Critical ESP32 ADC & GPIO 34 Properties
1. **Input-Only Silicon:** GPIO 34 is part of the ESP32 RTC/ADC1 domain. It lacks internal pull-up and pull-down transistors and cannot drive outputs. Software must strictly use `pinMode(34, INPUT)`.
2. **ADC1 vs ADC2:** GPIO 34 resides on **ADC1**. This is an intentional engineering choice: ADC1 remains 100% operational when ESP32 Wi-Fi is active. (ADC2 pins cannot be used while Wi-Fi is transmitting).
3. **11 dB Attenuation:** Configured via `analogSetAttenuation(ADC_11db)`. This sets the full-scale measurable voltage range to approximately $0.15\text{V} - 3.15\text{V}$, providing linear dynamic range for the 10 kΩ divider.

---

## 4. Glove Mechanical Mounting Recommendations

1. **MPU6050 Location:**
   - Mount securely to the dorsal (back) surface of the glove on the metacarpal region (flat back of the hand) using semi-rigid adhesive or a 3D-printed backing plate.
   - Orient the X-axis along the forearm pointing toward the knuckles, and the Z-axis pointing outward perpendicular to the back of the hand.
2. **FSR Location:**
   - Mount on the palmar surface directly over the second and third metacarpal heads (the base of the index/middle fingers) or along the interior grip channel where the barbell/dumbbell knurling exerts primary pressure.
   - Ensure the active sensor surface is backed by a firm layer to prevent edge-creasing.
