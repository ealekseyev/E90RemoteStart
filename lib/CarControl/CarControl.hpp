#ifndef CARCONTROL_HPP
#define CARCONTROL_HPP

#include <stdint.h>
#include <vector>
#include "canbus.h"

enum WindowPosition {
    WINDOW_NEUTRAL,
    WINDOW_ROLL_DOWN,
    WINDOW_ROLL_UP
};

enum IgnitionStatus {
    IGNITION_OFF,      // Engine flag off
    IGNITION_SECOND,   // Engine flag on, RPM < 400 (position 2)
    IGNITION_RUNNING   // Engine flag on, RPM >= 400 (engine running)
};

enum KeyState {
    KEY_ENGINE_OFF,        // 0x00: Engine off, key removed
    KEY_INSERTING,         // 0x40: Key being inserted (transitional)
    KEY_POSITION_1,        // 0x41: Key in position 1 (accessories only)
    KEY_POSITION_2,        // 0x45: Key in position 2 (engine running)
    KEY_CRANKING           // 0x55: Engine cranking or auto start-stop
};

enum GearPosition {
    GEAR_PARK,             // 0xE3: Park
    GEAR_REVERSE,          // 0xC2: Reverse
    GEAR_NEUTRAL,          // 0xD1: Neutral
    GEAR_DRIVE,            // 0xC7: Drive
    GEAR_DRIVE_SPORT,      // Drive Sport (if available)
    GEAR_UNKNOWN           // Unknown/invalid gear
};

// Window selection flags (can be OR'd together)
#define DRIVER_FRONT    0x01
#define PASSENGER_FRONT 0x02
#define DRIVER_REAR     0x04
#define PASSENGER_REAR  0x08

class CarControl {
public:
    static CarControl* getInstance();

    void init(CANBus* canBus);
    void update();  // MUST be called every loop cycle for non-blocking timing
    void onCanFrameReceived(const CANFrame& frame);

    // Read-only status functions
    bool isBraking() const;
    bool isDoorLocked() const;
    bool isDoorOpen(uint8_t doorMask = 0xFF) const;  // 0xFF = any door
    bool isDriverDoorOpen() const;
    bool areMirrorsRetracted() const;
    bool isParkingBrakeOn() const;
    bool isSteeringButtonPressed(uint8_t mask) const;
    bool isSeatBeltPlugged() const;
    uint8_t getBrakeStatus() const;
    uint8_t getDomeLightBrightness() const;
    float getBatteryVoltage() const;
    bool isEngineRunning() const;
    IgnitionStatus getIgnitionStatus() const;
    uint8_t getWindowPosition(uint8_t window) const;
    uint16_t getEngineRPM() const;
    uint8_t getThrottlePosition() const;
    float getSteeringWheelAngle() const;
    float getSpeed() const;  // MPH
    int8_t getEngineTemp() const;  // °C
    uint32_t getOdometer() const;  // KM
    float getRange() const;  // KM
    uint8_t getFuelLevel() const;  // Litres
    float getTorque() const;  // Nm
    float getPower() const;  // KW (calculated from RPM and torque)
    KeyState getKeyState() const;
    bool isEngineCranking() const;
    GearPosition getGearPosition() const;

    // Writable control functions
    bool setWindow(uint8_t windowMask, WindowPosition pos);
    bool setDomeLight(bool on);
    bool toggleTractionControl(bool completelyOff);
    bool sendFakeRPM(uint16_t rpm);
    bool spoofReverseLights();
    bool error(uint16_t name);

    // CAN frame sending (public for helper functions)
    bool sendCanFrame(uint16_t id, const uint8_t* data, uint8_t len);

    // non-returning actions
    void playGong();

private:
    CarControl();
    CarControl(const CarControl&) = delete;
    CarControl& operator=(const CarControl&) = delete;

    static CarControl* instance;
    CANBus* canBus;

    struct CarState {
        bool braking;
        bool doorLocked;
        bool doorOpen;
        bool driverDoorOpen;
        bool doorOpenDriverFront;
        bool doorOpenPassengerFront;
        bool doorOpenDriverRear;
        bool doorOpenPassengerRear;
        bool mirrorsRetracted;
        bool parkingBrakeOn;
        uint16_t steeringButtonPressed;
        bool seatBeltPlugged;
        uint8_t brakeStatus;
        uint8_t domeLightBrightness;
        float batteryVoltage;
        bool engineFlagFromCAN;  // Raw engine flag from 0x3B4 byte 2
        uint8_t windowPosDriverFront;
        uint8_t windowPosPassengerFront;
        uint8_t windowPosDriverRear;
        uint8_t windowPosPassengerRear;
        uint16_t engineRPM;
        uint8_t throttlePosition;
        float steeringWheelAngle;  // Degrees: positive=clockwise, negative=counter-clockwise
        float speed;  // MPH
        int8_t engineTemp;  // °C
        uint32_t odometer;  // KM
        float range;  // KM
        uint8_t fuelLevel;  // Litres
        float torque;  // Nm
        uint8_t keyStateRaw;          // Raw byte from 0x130 byte 0
        bool keyStateAvailable;       // True if 0x130 has been received
        uint8_t gearPositionRaw;      // Raw byte from 0x304 byte 0
    } state;

    // Vector of update functions (helpers) - each takes CarControl* and returns false when done
    std::vector<bool (*)(CarControl*)> updateFunctions;

    // Helper functions
    void parseCanFrame(uint16_t id, const uint8_t* data, uint8_t len);
    uint8_t getNibble(uint8_t byte, uint8_t nibbleIndex) const;
    void setBit(uint8_t& byte, uint8_t bitPos, bool value);
    bool getBit(uint8_t byte, uint8_t bitPos) const;

    // Placeholder handlers
    void updateFrame_0x0ea(const CANFrame& frame);
    void updateFrame_0x0ee(const CANFrame& frame);
};

#endif
