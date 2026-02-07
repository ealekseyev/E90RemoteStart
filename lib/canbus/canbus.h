#ifndef CANBUS_H
#define CANBUS_H

#include <stdint.h>
#include <mcp2515.h>

struct CANFrame {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};

class CANBus {
public:
    CANBus(uint8_t csPin);

    bool init(uint32_t bitrate);
    bool initInterrupt(uint32_t bitrate, uint8_t intPin);
    bool write(const CANFrame& frame);
    bool read(CANFrame& frame);
    void setControlObjects(void* car, void* climate);

    void handleInterrupt();  // Called by ISR

private:
    MCP2515 mcp;
    uint8_t interruptPin;
    bool interruptEnabled;
};

#endif
