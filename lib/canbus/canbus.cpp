#include "canbus.h"
#include "config.h"
#include <SPI.h>
#include <Arduino.h>

// Global pointer for ISR access
static CANBus* g_canBus = nullptr;

// ISR function
void IRAM_ATTR canISR() {
    if (g_canBus) {
        g_canBus->handleInterrupt();
    }
}

CANBus::CANBus(uint8_t csPin) : mcp(csPin), interruptPin(0), interruptEnabled(false),
                                 bufferHead(0), bufferTail(0) {}

bool CANBus::init(uint32_t bitrate) {
    SPI.begin();
    mcp.reset();

    CAN_SPEED speed;
    switch (bitrate) {
        case 100000:  speed = CAN_100KBPS; break;
        case 125000:  speed = CAN_125KBPS; break;
        case 250000:  speed = CAN_250KBPS; break;
        case 500000:  speed = CAN_500KBPS; break;
        case 1000000: speed = CAN_1000KBPS; break;
        default:      speed = CAN_125KBPS; break;
    }

    #if defined(MCP2515_CRYSTAL_16MHZ)
        CAN_CLOCK crystal = MCP_16MHZ;
    #elif defined(MCP2515_CRYSTAL_20MHZ)
        CAN_CLOCK crystal = MCP_20MHZ;
    #else
        CAN_CLOCK crystal = MCP_8MHZ;
    #endif

    if (mcp.setBitrate(speed, crystal) != MCP2515::ERROR_OK) {
        return false;
    }

    mcp.setNormalMode();
    return true;
}

bool CANBus::initInterrupt(uint32_t bitrate, uint8_t intPin) {
    if (!init(bitrate)) {
        return false;
    }

    interruptPin = intPin;
    interruptEnabled = true;
    g_canBus = this;

    // Configure interrupt pin
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), canISR, FALLING);

    return true;
}

bool CANBus::write(const CANFrame& frame) {
    struct can_frame txFrame;
    txFrame.can_id = frame.id;
    txFrame.can_dlc = frame.dlc;
    for (uint8_t i = 0; i < frame.dlc && i < 8; i++) {
        txFrame.data[i] = frame.data[i];
    }
    return mcp.sendMessage(&txFrame) == MCP2515::ERROR_OK;
}

bool CANBus::read(CANFrame& frame) {
    struct can_frame rxFrame;
    if (mcp.readMessage(&rxFrame) != MCP2515::ERROR_OK) {
        return false;
    }
    frame.id = rxFrame.can_id;
    frame.dlc = rxFrame.can_dlc;
    for (uint8_t i = 0; i < rxFrame.can_dlc && i < 8; i++) {
        frame.data[i] = rxFrame.data[i];
    }
    return true;
}

bool CANBus::readBuffered(CANFrame& frame) {
    return bufferGet(frame);
}

void IRAM_ATTR CANBus::handleInterrupt() {
    CANFrame frame;
    if (read(frame)) {
        bufferPut(frame);
    }
}

bool CANBus::bufferPut(const CANFrame& frame) {
    uint8_t nextHead = (bufferHead + 1) % CAN_BUFFER_SIZE;

    if (nextHead == bufferTail) {
        return false;  // Buffer overflow
    }

    // Copy field-by-field due to volatile
    buffer[bufferHead].id = frame.id;
    buffer[bufferHead].dlc = frame.dlc;
    for (uint8_t i = 0; i < 8; i++) {
        buffer[bufferHead].data[i] = frame.data[i];
    }
    bufferHead = nextHead;
    return true;
}

bool CANBus::bufferGet(CANFrame& frame) {
    if (bufferHead == bufferTail) {
        return false;  // Buffer empty
    }

    // Copy field-by-field due to volatile
    frame.id = buffer[bufferTail].id;
    frame.dlc = buffer[bufferTail].dlc;
    for (uint8_t i = 0; i < 8; i++) {
        frame.data[i] = buffer[bufferTail].data[i];
    }
    bufferTail = (bufferTail + 1) % CAN_BUFFER_SIZE;
    return true;
}
