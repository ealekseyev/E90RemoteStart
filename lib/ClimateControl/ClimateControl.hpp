#ifndef CLIMATE_CONTROL_HPP
#define CLIMATE_CONTROL_HPP

#include <Arduino.h>
#include <vector>
#include "canbus.h"

// Blower distribution flags (can be OR'd together)
#define BLOWER_WINDSHIELD  0x01
#define BLOWER_CENTER      0x02
#define BLOWER_FOOTWELL    0x04
#define BLOWER_AUTO        0x00  // Special case: auto mode

class ClimateControl {
public:
    // Singleton pattern
    static ClimateControl* getInstance();

    // Initialize with CAN bus reference
    void init(CANBus* canBus);

    // Called every loop cycle (for future timed commands)
    void update();

    // Process incoming CAN frames
    void onCanFrameReceived(const CANFrame& frame);

    // Getters
    uint8_t getFanSpeed() const;           // 0-7
    int8_t getDriverTemp() const;          // 16-28 degrees C
    int8_t getPassengerTemp() const;       // 16-28 degrees C
    bool isACActive() const;               // true if AC compressor running
    uint8_t getBlowerState() const;        // OR'd flags (BLOWER_*) or BLOWER_AUTO

    // Control functions
    bool setDriverSeatHeater(bool on);

    // CAN frame sending (public for helper functions)
    bool sendCanFrame(uint16_t id, const uint8_t* data, uint8_t len);

private:
    ClimateControl();
    static ClimateControl* instance;

    CANBus* canBus;

    // Climate state
    struct ClimateState {
        uint8_t fanSpeed;         // 0-7 (raw value from CAN)
        bool fanOn;               // true if fan actually running
        int8_t driverTemp;        // 16-28°C
        int8_t passengerTemp;     // 16-28°C
        bool acActive;            // AC compressor status
        uint8_t blowerState;      // OR'd BLOWER_* flags or BLOWER_AUTO
    } state;

    // Vector of update functions (helpers) - each takes ClimateControl* and returns false when done
    std::vector<bool (*)(ClimateControl*)> updateFunctions;

    // Parse CAN frames
    void parseCanFrame(uint16_t id, const uint8_t* data, uint8_t len);

    // Bit helper
    bool getBit(uint8_t byte, uint8_t bitPos) const;
};

#endif
