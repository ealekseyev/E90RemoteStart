# CAN Debugger Project

## Overview
ESP32-based CAN bus debugger and controller for BMW E90 vehicles. Enables reading vehicle state from K-CAN and sending control commands.

## Project Structure
```
CANDebugger/
└── esp/CANDebuggerFirmware/    # Main project directory (PlatformIO)
    ├── src/main.cpp             # Main application loop
    ├── lib/CarControl/          # Vehicle state management & CAN parsing
    ├── lib/CANBus/              # MCP2515 CAN interface driver
    ├── CAN_IDs.md               # **CAN protocol documentation**
    └── client/                  # Python CAN debugging tools
        ├── can_reader.py        # CAN frame reader/logger
        ├── can_sender.py        # CAN frame sender
        ├── nibble_monitor.py    # Nibble-level CAN analyzer
        └── blacklist*.json      # CAN ID filters
```

## Key Documentation
**IMPORTANT:** Always reference `CAN_IDs.md` for CAN protocol details, byte layouts, and implementation examples.

## Architecture

### Hardware
- **Platform:** ESP32
- **CAN Controller:** MCP2515 (SPI interface)
- **CAN Bus:** BMW K-CAN (500 kbps)

### Software Components

#### 1. CarControl (lib/CarControl/)
**Singleton class managing vehicle state**
- Parses incoming CAN frames → updates internal state
- Provides read-only getters for vehicle status
- Provides control methods for writable functions (windows, lights, etc.)
- Uses helper update functions for timed multi-frame sequences

**Key Pattern:**
```cpp
CarControl* carControl = CarControl::getInstance();
carControl->init(&can);
carControl->update();  // Call every loop for timing
carControl->onCanFrameReceived(frame);  // Pass CAN frames
```

#### 2. CANBus (lib/CANBus/)
**MCP2515 driver wrapper**
- Hardware abstraction for CAN operations
- Provides `read()`, `write()`, `init()` methods

#### 3. Main Loop (src/main.cpp)
- Reads CAN frames → passes to CarControl
- Prints vehicle status to serial (toggleable DEBUG_MODE)
- Accepts serial commands for manual CAN frame injection

## Important Implementation Notes

### Data Types
- **Multi-byte values:** Always little-endian format
  - Example: `raw = (data[highByteIndex] << 8) | data[lowByteIndex]`
- **Always validate length** before accessing bytes: `if (len > N)`
- Use appropriate types to prevent overflow in calculations (e.g., `uint32_t` for intermediate scaling)

### State Management
- All state in `CarControl::state` struct (zeroed on init)
- State updated only via `parseCanFrame()`
- Read-only outside CarControl (use getters)

### Control Commands
- Multi-frame commands use helper functions in `updateFunctions` vector
- Helpers manage timing state internally (static variables)
- Return `false` when complete (auto-removed from queue)

### Serial Interface
- **DEBUG_MODE defined:** Prints all raw CAN frames
- **DEBUG_MODE commented out:** Prints formatted vehicle status
- Accepts manual CAN injection: `xxx:yyyy...` (hex ID:data)

## Common Workflows

### Reading Vehicle State
1. CAN frame received in `main.cpp`
2. Passed to `carControl->onCanFrameReceived(frame)`
3. `parseCanFrame()` updates `state` struct
4. Access via getters: `carControl->getEngineRPM()`, etc.

### Sending Control Commands
1. Call control method: `carControl->setWindow(mask, position)`
2. Method validates CAN bus availability
3. For simple commands: Sends frame directly
4. For timed sequences: Adds helper to `updateFunctions`
5. `update()` manages helper lifecycle

### Adding New CAN IDs
1. Document in `CAN_IDs.md` first
2. Add state fields to `CarControl::CarState` struct
3. Add case in `parseCanFrame()` switch
4. Add getter methods (read-only) or control methods (writable)
5. Update main.cpp output if needed

## Development Guidelines

### Code Style
- **Concise:** No unnecessary comments or over-engineering
- **Direct:** Read files before modifying, understand existing patterns
- **Focused:** Only change what's needed for the task
- **Safe:** Validate lengths, check null pointers, avoid overflow

### Token Usage
- **Be concise** in explanations and plans
- **Don't repeat** information already in this file or CAN_IDs.md
- **Reference** documentation instead of duplicating it
- **Show code** instead of describing it extensively
- **Keep plans short** - bullet points, not essays

### Testing
- Compile: `platformio run`
- Upload: `platformio upload`
- Monitor: `platformio device monitor` (115200 baud)
- Inject test frames via serial when needed

## Quick Reference

### Current Implementations
- ✅ Engine RPM (0x0AA bytes 4-5)
- ✅ Throttle position (0x0AA bytes 2-3, scaled 0-255)
- ✅ Braking status (0x0A8)
- ✅ Door locks/open status (0x0E2, 0x0E6, 0x1E1)
- ✅ Mirrors retracted (0x0F6)
- ✅ Parking brake (0x1B4)
- ✅ Steering buttons (0x1D6)
- ✅ Battery voltage (0x3B4)
- ✅ Window positions (0x3B6-0x3B9)
- ✅ Window control (0x0FA)
- ✅ Dome light control (0x1E3)
- ✅ Seat heater control (0x1E7)

### Debug Mode Toggle
Comment/uncomment `#define DEBUG_MODE` in `src/main.cpp` line 10

---

**Instructions for Claude:**
- Read `esp/CANDebuggerFirmware/CAN_IDs.md` when working with CAN protocol
- Be concise and token-efficient in responses
- Focus on implementation, not explanation
- Keep plans short: use bullet points, avoid verbose descriptions
- Reference this file and CAN_IDs.md instead of repeating information
