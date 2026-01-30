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
    bool isDoorOpen() const;
    bool isDriverDoorOpen() const;
    bool areMirrorsRetracted() const;
    bool isParkingBrakeOn() const;
    bool isSteeringButtonPressed(uint8_t mask) const;
    bool isSeatBeltPlugged() const;
    uint8_t getBrakeStatus() const;
    uint8_t getDomeLightBrightness() const;
    float getBatteryVoltage() const;
    bool isEngineRunning() const;
    uint8_t getWindowPosition(uint8_t window) const;
    uint16_t getEngineRPM() const;
    uint8_t getThrottlePosition() const;

    // Writable control functions
    bool setWindow(uint8_t windowMask, WindowPosition pos);
    bool setDriverSeatHeater(bool on);
    bool setDomeLight(bool on);

    // CAN frame sending (public for helper functions)
    bool sendCanFrame(uint16_t id, const uint8_t* data, uint8_t len);

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
        bool mirrorsRetracted;
        bool parkingBrakeOn;
        uint16_t steeringButtonPressed;
        bool seatBeltPlugged;
        uint8_t brakeStatus;
        uint8_t domeLightBrightness;
        float batteryVoltage;
        bool engineRunning;
        uint8_t windowPosDriverFront;
        uint8_t windowPosPassengerFront;
        uint8_t windowPosDriverRear;
        uint8_t windowPosPassengerRear;
        uint16_t engineRPM;
        uint8_t throttlePosition;
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
