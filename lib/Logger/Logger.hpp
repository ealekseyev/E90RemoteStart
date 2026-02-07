#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <Arduino.h>
#include "canbus.h"

// Forward declarations
class CarControl;
class ClimateControl;

#define LOG_BUFFER_SIZE 32

struct LogEntry {
    CANFrame frame;
    uint32_t timestamp;  // millis() when received
};

class Logger {
public:
    static void diag(String str);
    static void caninfo(const CANFrame& frame, CarControl* carControl, ClimateControl* climateControl);

    // New functions for deferred logging
    static void bufferLogEntry(const CANFrame& frame);  // Called from ISR
    static bool processNextLog(CarControl* carControl, ClimateControl* climateControl);  // Called from main loop

private:
    static volatile LogEntry logBuffer[LOG_BUFFER_SIZE];
    static volatile uint8_t logHead;
    static volatile uint8_t logTail;

    static bool logBufferPut(const LogEntry& entry);
    static bool logBufferGet(LogEntry& entry);
};

#endif
