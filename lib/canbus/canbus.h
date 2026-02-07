#ifndef CANBUS_H
#define CANBUS_H

#include <stdint.h>
#include <mcp2515.h>

#define CAN_BUFFER_SIZE 32

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
    bool readBuffered(CANFrame& frame);

    void handleInterrupt();  // Called by ISR

private:
    MCP2515 mcp;
    uint8_t interruptPin;
    bool interruptEnabled;

    // Circular buffer for interrupt mode
    volatile CANFrame buffer[CAN_BUFFER_SIZE];
    volatile uint8_t bufferHead;
    volatile uint8_t bufferTail;

    bool bufferPut(const CANFrame& frame);
    bool bufferGet(CANFrame& frame);
};

#endif
