#include "canbus.h"
#include "CarControl.hpp"
#include "ClimateControl.hpp"
#include "Logger.hpp"
#include "config.h"
#include <SPI.h>
#include <Arduino.h>

// Global pointers for ISR access
static CANBus* g_canBus = nullptr;
static CarControl* g_carControl = nullptr;
static ClimateControl* g_climateControl = nullptr;

// ISR function
void IRAM_ATTR canISR() {
    if (g_canBus) {
        g_canBus->handleInterrupt();
    }
}

CANBus::CANBus(uint8_t csPin) : mcp(csPin), interruptPin(0), interruptEnabled(false) {}

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

void CANBus::setControlObjects(void* car, void* climate) {
    g_carControl = (CarControl*)car;
    g_climateControl = (ClimateControl*)climate;
}

void IRAM_ATTR CANBus::handleInterrupt() {
    CANFrame frame;
    if (read(frame)) {
        // Update state immediately (ISR context)
        if (g_carControl) {
            g_carControl->onCanFrameReceived(frame);
        }
        if (g_climateControl) {
            g_climateControl->onCanFrameReceived(frame);
        }

        // Buffer for deferred logging
        Logger::bufferLogEntry(frame);
    }
}
