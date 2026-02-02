#include "ClimateControl.hpp"
#include "config.h"

// Static instance
ClimateControl* ClimateControl::instance = nullptr;

// Minimal command struct for communication with helpers
static struct { bool on; bool pending; } seatHeaterCommand = {false, false};

// ============ Update Helper Functions (self-contained with static state) ============

static bool updateSeatHeaterControl(ClimateControl* ctrl) {
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

ClimateControl::ClimateControl() : canBus(nullptr) {
    // Zero-initialize state
    state.fanSpeed = 0;
    state.fanOn = false;
    state.driverTemp = 0;
    state.passengerTemp = 0;
    state.acActive = false;
    state.blowerState = BLOWER_AUTO;
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
                // Temp range: 16-28°C (11 degrees)
                uint8_t rawTemp = data[7];
                if (rawTemp >= 0x20 && rawTemp <= 0x38) {
                    state.driverTemp = 16 + ((rawTemp - 0x20) * 11) / 24;
                }
            }
            break;

        case 0x2ea:  // Passenger temperature
            // Byte 7: Passenger temperature (same formula as driver)
            if (len > 7) {
                // Raw range: 0x20-0x38 (24 steps)
                // Temp range: 16-28°C (11 degrees)
                uint8_t rawTemp = data[7];
                if (rawTemp >= 0x20 && rawTemp <= 0x38) {
                    state.passengerTemp = 16 + ((rawTemp - 0x20) * 11) / 24;
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

// Bit helper (same as CarControl)
bool ClimateControl::getBit(uint8_t byte, uint8_t bitPos) const {
    return (byte & (1 << bitPos)) != 0;
}

// Control functions
bool ClimateControl::setDriverSeatHeater(bool on) {
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
