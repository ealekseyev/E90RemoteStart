#include "ClimateControl.hpp"
#include "config.h"

// Static instance
ClimateControl* ClimateControl::instance = nullptr;

ClimateControl::ClimateControl() : canBus(nullptr) {
    // Zero-initialize state
    state.fanSpeed = 0;
    state.fanOn = false;
    state.driverTemp = 0;
    state.passengerTemp = 0;
    state.acActive = false;
    state.blowerState = BLOWER_AUTO;
    state.driverSeatHeater = 0;
    state.passengerSeatHeater = 0;
}

ClimateControl* ClimateControl::getInstance() {
    if (instance == nullptr) {
        instance = new ClimateControl();
    }
    return instance;
}

void ClimateControl::init(CANBus* bus) {
    canBus = bus;
}

void ClimateControl::update() {
    // Process all queued update functions
    for (size_t i = 0; i < updateFunctions.size(); ) {
        if (!updateFunctions[i](this)) {
            // Function returned false (done), remove it
            updateFunctions.erase(updateFunctions.begin() + i);
        } else {
            // Function still running, keep it
            i++;
        }
    }
}

void ClimateControl::onCanFrameReceived(const CANFrame& frame) {
    parseCanFrame(frame.id, frame.data, frame.dlc);
}

void ClimateControl::parseCanFrame(uint16_t id, const uint8_t* data, uint8_t len) {
    switch (id) {
        case 0x2e6:  // Fan speed, driver temperature, and blower distribution
            // Bytes 0-2: Blower distribution
            if (len > 2) {
                // Check for AUTO mode: 00 64 1E
                if (data[0] == 0x00 && data[1] == 0x64 && data[2] == 0x1E) {
                    state.blowerState = BLOWER_AUTO;
                } else {
                    // Manual mode: OR together active blowers
                    state.blowerState = 0;
                    if (data[0] > 0) state.blowerState |= BLOWER_WINDSHIELD;
                    if (data[1] > 0) state.blowerState |= BLOWER_CENTER;
                    if (data[2] > 0) state.blowerState |= BLOWER_FOOTWELL;

                    // If all zeros and not auto pattern, treat as auto
                    if (state.blowerState == 0) {
                        state.blowerState = BLOWER_AUTO;
                    }
                }
            }

            // Byte 5: Fan speed (0-7)
            if (len > 5) {
                state.fanSpeed = data[5] & 0x07;  // Mask to 0-7
            }

            // Byte 7: Driver temperature (0x20 = 16°C, 0x38 = 28°C)
            if (len > 7) {
                // Raw range: 0x20-0x38 (24 steps)
                // Temp range: 16-28°C (12 degrees)
                uint8_t rawTemp = data[7];
                if (rawTemp >= 0x20 && rawTemp <= 0x38) {
                    state.driverTemp = 16 + ((rawTemp - 0x20) * 12) / 24;
                }
            }
            break;

        case 0x2ea:  // Passenger temperature
            // Byte 7: Passenger temperature (same formula as driver)
            if (len > 7) {
                // Raw range: 0x20-0x38 (24 steps)
                // Temp range: 16-28°C (12 degrees)
                uint8_t rawTemp = data[7];
                if (rawTemp >= 0x20 && rawTemp <= 0x38) {
                    state.passengerTemp = 16 + ((rawTemp - 0x20) * 12) / 24;
                }
            }
            break;

        case 0x242:  // AC active status and fan on/off
            // Byte 0, bit 0: AC active (0x10 vs 0x11)
            if (len > 0) {
                state.acActive = getBit(data[0], 0);
            }
            // Byte 2, bit 0: Fan on/off (0xF0=off, 0xF1=on)
            if (len > 2) {
                state.fanOn = getBit(data[2], 0);
            }
            break;

        case 0x232:  // Driver seat heater status
            // Byte 0, nibble 0: Driver seat heater level (0=off, 1=low, 2=medium, 3=high)
            if (len > 0) {
                state.driverSeatHeater = (data[0] & 0xF0) >> 4;  // Lower nibble
            }
            break;

        case 0x22a:  // Passenger seat heater status
            // Byte 0, nibble 1: Passenger seat heater level (0=off, 1=low, 2=medium, 3=high)
            if (len > 0) {
                state.passengerSeatHeater = (data[0] & 0xF0) >> 4;  // Upper nibble
            }
            break;
    }
}

// Getters
uint8_t ClimateControl::getFanSpeed() const {
    // Only use fanOn flag to distinguish between 0 and 1
    // CAN shows minimum of 1 even when fan is off
    if (state.fanSpeed == 1 && !state.fanOn) {
        return 0;
    }
    return state.fanSpeed;
}

int8_t ClimateControl::getDriverTemp() const {
    return state.driverTemp;
}

int8_t ClimateControl::getPassengerTemp() const {
    return state.passengerTemp;
}

bool ClimateControl::isACActive() const {
    return state.acActive;
}

uint8_t ClimateControl::getBlowerState() const {
    return state.blowerState;
}

uint8_t ClimateControl::getDriverSeatHeaterLevel() const {
    return state.driverSeatHeater;
}

uint8_t ClimateControl::getPassengerSeatHeaterLevel() const {
    return state.passengerSeatHeater;
}

// Bit helper (same as CarControl)
bool ClimateControl::getBit(uint8_t byte, uint8_t bitPos) const {
    return (byte & (1 << bitPos)) != 0;
}

// Control functions
bool ClimateControl::setDriverSeatHeater(uint8_t desiredLevel) {
    if (!canBus || desiredLevel > 3) return false;

    // Heater cycle: 0→3→2→1→0
    uint8_t currentPos = state.driverSeatHeater == 0 ? 0 : 4 - state.driverSeatHeater;
    uint8_t desiredPos = desiredLevel == 0 ? 0 : 4 - desiredLevel;
    uint8_t clicksNeeded = (desiredPos - currentPos + 4) % 4;

    if (clicksNeeded == 0) return true;

    for (uint8_t i = 0; i < clicksNeeded; i++) {
        toggleDriverSeatHeater();
        delay(80);
    }

    return true;
}

bool ClimateControl::toggleDriverSeatHeater() {
    if (!canBus) return false;

    // Send button press
    uint8_t pressData[8] = {0xfd, 0xff};
    sendCanFrame(0x1e7, pressData, 2);
    delay(80);

    // Send button release
    uint8_t releaseData[8] = {0xfc, 0xff};
    sendCanFrame(0x1e7, releaseData, 2);

    return true;

}

bool ClimateControl::setPassengerSeatHeater(uint8_t desiredLevel) {
    if (!canBus || desiredLevel > 3) return false;

    // Heater cycle: 0→3→2→1→0
    uint8_t currentPos = state.passengerSeatHeater == 0 ? 0 : 4 - state.passengerSeatHeater;
    uint8_t desiredPos = desiredLevel == 0 ? 0 : 4 - desiredLevel;
    uint8_t clicksNeeded = (desiredPos - currentPos + 4) % 4;

    if (clicksNeeded == 0) return true;

    for (uint8_t i = 0; i < clicksNeeded; i++) {
        togglePassengerSeatHeater();
        if (i < clicksNeeded - 1) delay(BUTTON_PRESS_DURATION_MS);
    }

    return true;
}

bool ClimateControl::togglePassengerSeatHeater() {
    if (!canBus) return false;

    // Send button press
    uint8_t pressData[8] = {0xfd, 0xff};
    sendCanFrame(0x1e8, pressData, 1);

    // Wait
    delay(BUTTON_PRESS_DURATION_MS);

    // Send button release
    uint8_t releaseData[8] = {0xfc, 0xff};
    sendCanFrame(0x1e8, releaseData, 1);

    return true;
}

bool ClimateControl::sendCanFrame(uint16_t id, const uint8_t* data, uint8_t len) {
    if (!canBus) return false;

    CANFrame frame;
    frame.id = id;
    frame.dlc = len;
    for (uint8_t i = 0; i < len && i < 8; i++) {
        frame.data[i] = data[i];
    }

    canBus->write(frame);
    return true;
}
