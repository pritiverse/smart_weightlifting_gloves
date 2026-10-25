#ifndef ARDUINO_COMPAT_H
#define ARDUINO_COMPAT_H

/**
 * ============================================================================
 * ARDUINO & ESP32 DESKTOP COMPATIBILITY SHIM (IDE STATIC ANALYSIS / CLANGD)
 * ============================================================================
 * This header is included ONLY when analyzing outside the Arduino ESP32 toolchain.
 * When built with Arduino IDE or arduino-cli, native ESP32 core headers are used.
 */

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef decltype(sizeof(0)) size_t;

inline void* memset(void* s, int c, size_t n) { return s; }
inline char* strncpy(char* dest, const char* src, size_t n) { return dest; }
inline int strcmp(const char* s1, const char* s2) { return 0; }
inline size_t strlen(const char* s) { return 0; }

inline float sqrtf(float x) { return x; }
inline float fabsf(float x) { return x < 0 ? -x : x; }
inline float atan2f(float y, float x) { return 0.0f; }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef constrain
#define constrain(amt, low, high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#ifndef F
#define F(str) (str)
#endif

// Pin modes & ADC definitions
#define INPUT 0x0
#define OUTPUT 0x1
#define ADC_11db 3

inline void pinMode(uint8_t, uint8_t) {}
inline void analogReadResolution(uint8_t) {}
inline void analogSetAttenuation(uint8_t) {}
inline void analogSetPinAttenuation(uint8_t, uint8_t) {}
inline int analogRead(uint8_t) { return 0; }
inline uint32_t millis() { return 0; }
inline uint32_t micros() { return 0; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}

// Arduino String Stub
class String {
public:
    String() {}
    String(const char*) {}
    String(int) {}
    String(unsigned int) {}
    String(long) {}
    String(unsigned long) {}
    String(float, int = 2) {}
    String(double, int = 2) {}
    String& operator+=(const String&) { return *this; }
    String& operator+=(const char*) { return *this; }
    String operator+(const String&) const { return *this; }
    String operator+(const char*) const { return *this; }
    const char* c_str() const { return ""; }
    int length() const { return 0; }
    unsigned char reserve(unsigned int) { return 1; }
    int toInt() const { return 0; }
    float toFloat() const { return 0.0f; }
    bool operator==(const char*) const { return false; }
    bool operator==(const String&) const { return false; }
};
inline String operator+(const char*, const String&) { return String(); }

// IPAddress Stub
class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
    const char* toString() const { return "0.0.0.0"; }
};

// I2C Wire Stub
class TwoWireStub {
public:
    bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) { return true; }
    void setClock(uint32_t) {}
    void setTimeOut(uint16_t) {}
    void setTimeout(uint32_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t write(uint8_t) { return 1; }
    uint8_t endTransmission(bool sendStop = true) { return 0; }
    uint8_t requestFrom(uint8_t, uint8_t, bool sendStop = true) { return 0; }
    int available() { return 0; }
    int read() { return 0; }
};
inline TwoWireStub Wire;

// Hardware Serial Stub
class HardwareSerialStub {
public:
    void begin(unsigned long) {}
    void print(const char*) {}
    void print(const String&) {}
    void print(const IPAddress&) {}
    void print(int) {}
    void print(float, int = 2) {}
    void println(const char* = "") {}
    void println(const String&) {}
    void println(const IPAddress&) {}
    void println(int) {}
    void println(float, int = 2) {}
    int printf(const char*, ...) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
};
inline HardwareSerialStub Serial;

// Preferences Stub (ESP32 Non-Volatile Storage)
class Preferences {
public:
    bool begin(const char*, bool = false) { return true; }
    void end() {}
    bool isKey(const char*) { return false; }
    size_t getBytes(const char*, void*, size_t) { return 0; }
    size_t putBytes(const char*, const void*, size_t) { return 0; }
    bool clear() { return true; }
};

// Wi-Fi Stub
#define WIFI_AP 2
#define WIFI_AP_STA 3
#define WL_CONNECTED 3

class WiFiClassStub {
public:
    bool mode(int) { return true; }
    bool setSleep(bool) { return true; }
    bool softAP(const char*, const char* = nullptr, int = 1, int = 0, int = 4) { return true; }
    IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
    void begin(const char*, const char*) {}
    int status() { return WL_CONNECTED; }
    IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
};
inline WiFiClassStub WiFi;

// WebServer Stub
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_OPTIONS 4

class WebServer {
public:
    WebServer(int = 80) {}
    template <typename F>
    void on(const char*, int, F) {}
    template <typename F>
    void on(const char*, F) {}
    template <typename F>
    void onNotFound(F) {}
    void begin() {}
    void handleClient() {}
    void send(int, const char* = "", const char* = "") {}
    void send(int, const char*, const String&) {}
    void sendHeader(const char*, const char*, bool = false) {}
    bool hasArg(const char*) { return false; }
    String arg(const char*) { return String(); }
    int method() { return HTTP_GET; }
};

#endif // ARDUINO_COMPAT_H
