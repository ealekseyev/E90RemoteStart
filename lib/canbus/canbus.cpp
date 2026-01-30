#include "canbus.h"
#include "config.h"
#include <SPI.h>

CANBus::CANBus(uint8_t csPin) : mcp(csPin) {}

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
