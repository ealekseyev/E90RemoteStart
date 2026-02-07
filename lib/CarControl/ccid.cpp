#include "CarControl.hpp"

bool CarControl::error(uint16_t name) {
    if (!canBus) return false;

    uint8_t data[8] = {
        (uint8_t)(name & 0xFF),        // Low byte
        (uint8_t)((name >> 8) & 0xFF), // High byte
        0x20, 0xF0, 0x00, 0xFE, 0xFE, 0xFE
    };

    return sendCanFrame(0x338, data, 8);
}
