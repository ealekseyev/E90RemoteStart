#include "Logger.hpp"
#include "CarControl.hpp"
#include "ClimateControl.hpp"
#include "config.h"

// Static member initialization
volatile LogEntry Logger::logBuffer[LOG_BUFFER_SIZE];
volatile uint8_t Logger::logHead = 0;
volatile uint8_t Logger::logTail = 0;

void Logger::diag(String str) {
    Serial.println("DIAG: " + str);
}

void Logger::caninfo(const CANFrame& frame, CarControl* carControl, ClimateControl* climateControl) {
#ifdef DEBUG_MODE
    // Raw CAN frame output
    Serial.print("RX: 0x");
    if (frame.id < 0x100) Serial.print("0");
    if (frame.id < 0x10) Serial.print("0");
    Serial.print(frame.id, HEX);
    Serial.print(" Data:");

    for (uint8_t i = 0; i < frame.dlc; i++) {
        Serial.print(" ");
        if (frame.data[i] < 0x10) Serial.print("0");
        Serial.print(frame.data[i], HEX);
    }
    Serial.println();
#else
    // Formatted vehicle status output
    Serial.printf("Engine ");
    IgnitionStatus ignition = carControl->getIgnitionStatus();
    switch (ignition) {
        case IGNITION_OFF:
            Serial.printf("OFF");
            break;
        case IGNITION_SECOND:
            Serial.printf("SECOND");
            break;
        case IGNITION_RUNNING:
            Serial.printf("RUNNING");
            break;
    }
    Serial.printf(", Battery: %0.2fV", carControl->getBatteryVoltage());
    Serial.printf(", RPM: %u", carControl->getEngineRPM());

    uint8_t throttle = carControl->getThrottlePosition();
    if (throttle == 255) {
        Serial.printf(", Throttle: KICKDOWN");
    } else {
        uint8_t percent = (throttle * 100) / 254;
        Serial.printf(", Throttle: %u%%", percent);
    }

    Serial.printf(", Steering: %.1f°", carControl->getSteeringWheelAngle());

    Serial.printf(", Climate - Fan: %u", climateControl->getFanSpeed());
    Serial.printf(" | Driver: %dC", climateControl->getDriverTemp());
    Serial.printf(" | Passenger: %dC", climateControl->getPassengerTemp());
    Serial.printf(" | AC: %s", climateControl->isACActive() ? "ON" : "OFF");
    Serial.println();
#endif
}

void Logger::bufferLogEntry(const CANFrame& frame) {
    LogEntry entry;
    entry.frame = frame;
    entry.timestamp = millis();
    logBufferPut(entry);
}

bool Logger::logBufferPut(const LogEntry& entry) {
    uint8_t nextHead = (logHead + 1) % LOG_BUFFER_SIZE;

    if (nextHead == logTail) {
        return false;  // Buffer full, drop log entry (state still updated!)
    }

    // Copy field-by-field due to volatile
    logBuffer[logHead].frame.id = entry.frame.id;
    logBuffer[logHead].frame.dlc = entry.frame.dlc;
    for (uint8_t i = 0; i < 8; i++) {
        logBuffer[logHead].frame.data[i] = entry.frame.data[i];
    }
    logBuffer[logHead].timestamp = entry.timestamp;

    logHead = nextHead;
    return true;
}

bool Logger::logBufferGet(LogEntry& entry) {
    if (logHead == logTail) {
        return false;  // Buffer empty
    }

    // Copy field-by-field due to volatile
    entry.frame.id = logBuffer[logTail].frame.id;
    entry.frame.dlc = logBuffer[logTail].frame.dlc;
    for (uint8_t i = 0; i < 8; i++) {
        entry.frame.data[i] = logBuffer[logTail].frame.data[i];
    }
    entry.timestamp = logBuffer[logTail].timestamp;

    logTail = (logTail + 1) % LOG_BUFFER_SIZE;
    return true;
}

bool Logger::processNextLog(CarControl* carControl, ClimateControl* climateControl) {
    LogEntry entry;
    if (!logBufferGet(entry)) {
        return false;  // No logs to process
    }

    // Now in main loop context - safe to call Serial I/O
    caninfo(entry.frame, carControl, climateControl);
    return true;
}
