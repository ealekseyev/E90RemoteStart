#include "CarControl.hpp"
#include "config.h"
#include <string.h>
#include <Arduino.h>

CarControl* CarControl::instance = nullptr;

// Minimal command structs for communication with helpers
static struct { bool on; bool pending; } seatHeaterCommand = {false, false};
static struct { bool pending; } domeLightCommand = {false};

// ============ Update Helper Functions (self-contained with static state) ============

static bool updateSeatHeaterControl(CarControl* ctrl) {
    // All state is static inside this function
    static unsigned long pressTime = 0;
    static bool active = false;

    // Handle new command
    if (seatHeaterCommand.pending) {
        seatHeaterCommand.pending = false;
        active = true;
        pressTime = millis();

        // Send press frame
        uint8_t pressData[8] = {0};
        pressData[0] = seatHeaterCommand.on ? 0xd0 : 0xc0;
        ctrl->sendCanFrame(0x1e7, pressData, 1);
    }

    if (!active) return false;

    unsigned long currentTime = millis();

    // Check if it's time to send release frame
    if (currentTime - pressTime >= BUTTON_PRESS_DURATION_MS) {
        uint8_t releaseData[8] = {0xc0};
        ctrl->sendCanFrame(0x1e7, releaseData, 1);
        active = false;
        return false;  // Done
    }

    return true;  // Continue
}

static bool updateDomeLightControl(CarControl* ctrl) {
    // All state is static inside this function
    static unsigned long pressTime = 0;
    static bool active = false;

    // Handle new command
    if (domeLightCommand.pending) {
        domeLightCommand.pending = false;
        active = true;
        pressTime = millis();

        // Send press frame
        uint8_t pressData[8] = {0xf1, 0xff, 0, 0, 0, 0, 0, 0};
        ctrl->sendCanFrame(0x1e3, pressData, 2);
    }

    if (!active) return false;

    unsigned long currentTime = millis();

    // Check if it's time to send release frame
    if (currentTime - pressTime >= BUTTON_PRESS_DURATION_MS) {
        uint8_t releaseData[8] = {0xf0, 0xff, 0, 0, 0, 0, 0, 0};
        ctrl->sendCanFrame(0x1e3, releaseData, 2);
        active = false;
        return false;  // Done
    }

    return true;  // Continue
}

CarControl::CarControl() : canBus(nullptr) {
    memset(&state, 0, sizeof(state));
}

CarControl* CarControl::getInstance() {
    if (instance == nullptr) {
        instance = new CarControl();
    }
    return instance;
}

void CarControl::init(CANBus* bus) {
    canBus = bus;
}

void CarControl::update() {
    // Iterate through all update functions and call them
    // Remove functions that return false (completed)
    for (auto it = updateFunctions.begin(); it != updateFunctions.end(); ) {
        if ((*it)(this)) {
            ++it;  // Keep this function
        } else {
            it = updateFunctions.erase(it);  // Remove completed function
        }
    }
}

void CarControl::onCanFrameReceived(const CANFrame& frame) {
    parseCanFrame(frame.id, frame.data, frame.dlc);
}

// ============ Read-only Status Functions ============

bool CarControl::isBraking() const {
    return state.braking;
}

bool CarControl::isDoorLocked() const {
    return state.doorLocked;
}

bool CarControl::isDoorOpen() const {
    return state.doorOpen;
}

bool CarControl::isDriverDoorOpen() const {
    return state.driverDoorOpen;
}

bool CarControl::areMirrorsRetracted() const {
    return state.mirrorsRetracted;
}

bool CarControl::isParkingBrakeOn() const {
    return state.parkingBrakeOn;
}

void print_8bit_binary(unsigned char num) {
    for (int i = 7; i >= 0; i--) {
        // Use bitwise right shift and AND to check each bit
        int bit = (num >> i) & 1;
        Serial.printf("%d", bit);
    }
}

bool CarControl::isSteeringButtonPressed(uint8_t mask) const {
    uint8_t buttonStates = 0;
    if(state.steeringButtonPressed & 0b0000100000000000) buttonStates |= STEERING_BTN_VOLUME_UP;
    if(state.steeringButtonPressed & 0b0000010000000000) buttonStates |= STEERING_BTN_VOLUME_DOWN;
    if(state.steeringButtonPressed & 0b0000000000000001) buttonStates |= STEERING_BTN_SIRI;
    if(state.steeringButtonPressed & 0b0000000100000000) buttonStates |= STEERING_BTN_PHONE;

    if(state.steeringButtonPressed & 0b0000000001000000) buttonStates |= STEERING_BTN_CUSTOM;
    if(state.steeringButtonPressed & 0b0000000000010000) buttonStates |= STEERING_BTN_CHANNEL;
    if(state.steeringButtonPressed & 0b0010000000000000) buttonStates |= STEERING_BTN_PREV;
    if(state.steeringButtonPressed & 0b0001000000000000) buttonStates |= STEERING_BTN_NEXT;
    return (buttonStates & mask) > 0;
}

bool CarControl::isSeatBeltPlugged() const {
    return state.seatBeltPlugged;
}

uint8_t CarControl::getBrakeStatus() const {
    return state.brakeStatus;
}

uint8_t CarControl::getDomeLightBrightness() const {
    return state.domeLightBrightness;
}

float CarControl::getBatteryVoltage() const {
    return state.batteryVoltage;
}

bool CarControl::isEngineRunning() const {
    return state.engineRunning;
}

uint8_t CarControl::getWindowPosition(uint8_t window) const {
    // Return position for first matching window in mask
    if (window & DRIVER_FRONT) return state.windowPosDriverFront;
    if (window & PASSENGER_FRONT) return state.windowPosPassengerFront;
    if (window & DRIVER_REAR) return state.windowPosDriverRear;
    if (window & PASSENGER_REAR) return state.windowPosPassengerRear;
    return 0;
}

uint16_t CarControl::getEngineRPM() const {
    return state.engineRPM;
}

uint8_t CarControl::getThrottlePosition() const {
    return state.throttlePosition;
}

// ============ Writable Control Functions ============

bool CarControl::setWindow(uint8_t windowMask, WindowPosition pos) {
    if (!canBus) return false;

    uint8_t data[3] = {0xc0, 0xc0, 0xff};

    // Bit positions for window control
    const uint8_t LEFT_DOWN = 0x02;  // Bit 1
    const uint8_t LEFT_UP   = 0x04;  // Bit 2
    const uint8_t RIGHT_DOWN = 0x10; // Bit 4
    const uint8_t RIGHT_UP   = 0x20; // Bit 5

    // Process each window in the mask
    if (windowMask & DRIVER_FRONT) {
        if (pos == WINDOW_ROLL_DOWN) data[0] |= LEFT_DOWN;
        else if (pos == WINDOW_ROLL_UP) data[0] |= LEFT_UP;
    }

    if (windowMask & PASSENGER_FRONT) {
        if (pos == WINDOW_ROLL_DOWN) data[0] |= RIGHT_DOWN;
        else if (pos == WINDOW_ROLL_UP) data[0] |= RIGHT_UP;
    }

    if (windowMask & DRIVER_REAR) {
        if (pos == WINDOW_ROLL_DOWN) data[1] |= LEFT_DOWN;
        else if (pos == WINDOW_ROLL_UP) data[1] |= LEFT_UP;
    }

    if (windowMask & PASSENGER_REAR) {
        if (pos == WINDOW_ROLL_DOWN) data[1] |= RIGHT_DOWN;
        else if (pos == WINDOW_ROLL_UP) data[1] |= RIGHT_UP;
    }

    // Send single frame
    return sendCanFrame(0x0fa, data, 3);
}

bool CarControl::setDriverSeatHeater(bool on) {
    if (!canBus) return false;

    // Set command for helper
    seatHeaterCommand.on = on;
    seatHeaterCommand.pending = true;

    // Add helper to queue if not already there
    bool alreadyInQueue = false;
    for (auto fn : updateFunctions) {
        if (fn == &updateSeatHeaterControl) {
            alreadyInQueue = true;
            break;
        }
    }

    if (!alreadyInQueue) {
        updateFunctions.push_back(&updateSeatHeaterControl);
    }

    return true;
}

bool CarControl::setDomeLight(bool on) {
    if (!canBus) return false;

    // Check current state: dome light brightness > 50 means it's on
    bool currentlyOn = (state.domeLightBrightness > 50);

    // If already in desired state, do nothing
    if (currentlyOn == on) {
        return true;
    }

    // Set command for helper to toggle
    domeLightCommand.pending = true;

    // Add helper to queue if not already there
    bool alreadyInQueue = false;
    for (auto fn : updateFunctions) {
        if (fn == &updateDomeLightControl) {
            alreadyInQueue = true;
            break;
        }
    }

    if (!alreadyInQueue) {
        updateFunctions.push_back(&updateDomeLightControl);
    }

    return true;
}

uint8_t min(uint8_t a, uint8_t b) {
    return a > b ? b:a;
}
// ============ Helper Functions ============

void CarControl::parseCanFrame(uint16_t id, const uint8_t* data, uint8_t len) {
    if (len == 0) return;

    switch(id) {
        case 0x0a8:  // Braking
            if (len > 1) {
                state.braking = (getNibble(data[1], 1) == 6);
            }
            break;

        case 0x0aa:  // Engine RPM and throttle position
            if (len > 5) {
                // RPM: bytes 4-5, byte 5 is MSB
                uint16_t rawRPM = (data[5] << 8) | data[4];
                state.engineRPM = rawRPM / 4;
            }
            if (len > 3) {
                // Throttle position: bytes 2-3 (raw range 255-65064)
                uint16_t rawThrottle = (data[3] << 8) | data[2];

                // Check for kickdown (byte 6)
                bool kickdown = (len > 6 && data[6] == 0xB4);

                if (kickdown) {
                    state.throttlePosition = 255;  // Reserved for kickdown
                } else {
                    // Scale from 255-65064 to 0-254
                    // Handle edge case: if raw is exactly 255 (foot off), set to 0
                    if (rawThrottle <= 255) {
                        state.throttlePosition = 0;
                    } else {
                        // Scale: ((raw - 255) * 254) / 64809
                        uint32_t scaled = ((uint32_t)(rawThrottle - 255) * 254) / 64809;
                        state.throttlePosition = (uint8_t)(scaled > 254 ? 254 : scaled);
                    }
                }
            }
            break;

        case 0x0e2:  // Central locking status
            state.doorLocked = (data[0] == 2);
            break;

        case 0x0e6:  // Door open/closed
            if (len > 2) {
                state.doorOpen = (data[2] == 0xfd);
            }
            break;

        case 0x0f6:  // Mirrors retracted
            state.mirrorsRetracted = (data[0] == 0xf3);
            break;

        case 0x1b4:  // Parking brake
            if (len > 5) {
                state.parkingBrakeOn = (data[5] == 0x32);
            }
            break;

        case 0x1d6:  // Steering button
            if (len > 1) {
                // Serial.print("raw data: ");
                // print_8bit_binary(data[0]);
                // print_8bit_binary(data[1]);
                // print_8bit_binary(data[2]);
                // print_8bit_binary(data[3]);
                // Serial.println();
                state.steeringButtonPressed = data[0] << 8 | data[1];
            }
            break;

        case 0x1e1:  // Driver door
            if (len > 2) {
                state.driverDoorOpen = (getNibble(data[2], 0) == 1);
            }
            break;

        case 0x286:  // Dome light brightness
            if (len > 1) {
                state.domeLightBrightness = data[1];
            }
            break;

        case 0x2b2:  // Brake status (detailed)
            if (len > 0) {
                state.brakeStatus = data[0];
            }
            break;

        case 0x2f1:  // Seat belt
            if (len > 2) {
                state.seatBeltPlugged = (getNibble(data[2], 0) == 0xd);
            }
            break;

        case 0x3b4:  // Battery voltage and engine status
            if (len > 1) {
                uint16_t raw = (data[1] << 8) | data[0];
                state.batteryVoltage = (raw - 0xf000) / 68.0;
            }
            if (len > 2) {
                state.engineRunning = (data[2] == 0x00);
            }
            break;

        case 0x3b6:  // Driver front window position
            if (len > 0) {
                state.windowPosDriverFront = (min(data[0], 0x50) * 255) / 0x50;
            }
            break;

        case 0x3b7:  // Driver rear window position
            if (len > 0) {
                state.windowPosDriverRear = (min(data[0], 0x50) * 255) / 0x50;
            }
            break;

        case 0x3b8:  // Passenger front window position
            if (len > 0) {
                state.windowPosPassengerFront = (min(data[0], 0x50) * 255) / 0x50;
            }
            break;

        case 0x3b9:  // Passenger rear window position
            if (len > 0) {
                state.windowPosPassengerRear = (min(data[0], 0x50) * 255) / 0x50;
            }
            break;

        case 0x0ea:  // Placeholder
            updateFrame_0x0ea(CANFrame{id, len, {data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]}});
            break;

        case 0x0ee:  // Placeholder
            updateFrame_0x0ee(CANFrame{id, len, {data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]}});
            break;

        default:
            // Unknown CAN ID, ignore
            break;
    }
}

bool CarControl::sendCanFrame(uint16_t id, const uint8_t* data, uint8_t len) {
    if (!canBus) return false;

    CANFrame frame;
    frame.id = id;
    frame.dlc = len;
    for (uint8_t i = 0; i < len && i < 8; i++) {
        frame.data[i] = data[i];
    }

    return canBus->write(frame);
}

uint8_t CarControl::getNibble(uint8_t byte, uint8_t nibbleIndex) const {
    if (nibbleIndex == 0) {
        return byte & 0x0F;  // Lower nibble
    } else {
        return (byte >> 4) & 0x0F;  // Upper nibble
    }
}

void CarControl::setBit(uint8_t& byte, uint8_t bitPos, bool value) {
    if (value) {
        byte |= (1 << bitPos);
    } else {
        byte &= ~(1 << bitPos);
    }
}

bool CarControl::getBit(uint8_t byte, uint8_t bitPos) const {
    return (byte & (1 << bitPos)) != 0;
}

// ============ Placeholder Functions ============

void CarControl::updateFrame_0x0ea(const CANFrame& frame) {
    // TODO: Implement when function is discovered
    (void)frame;  // Suppress unused parameter warning
}

void CarControl::updateFrame_0x0ee(const CANFrame& frame) {
    // TODO: Implement when function is discovered
    (void)frame;  // Suppress unused parameter warning
}
