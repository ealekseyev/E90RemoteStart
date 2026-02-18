# BMW E90 Remote Start & Smart Features

ESP8266-based CAN bus controller for adding advanced features to BMW E90 vehicles via K-CAN integration.

## Project Goals

**Planned Features:**
- 🔑 **Remote start** via stock keyfob (unlock sequence detection)
- 🔒 **Smart immobilizer** - locks ignition unless phone is present via Bluetooth
- 📡 **4G connectivity** - API for remote control & integration (e.g., OpenClaw assistant voice commands)

**Current Status:** CAN bus debugger & controller - reads vehicle state and can control windows, lights, climate, etc.

## Hardware

- **MCU:** ESP8266 (Adafruit Huzzah)
- **CAN Controller:** MCP2515 (SPI)
- **CAN Bus:** BMW K-CAN (500 kbps)

## Features

### Currently Implemented

**Vehicle State Reading:**
- Engine RPM, throttle position, braking status
- Steering wheel angle & button inputs
- Door locks/open status, parking brake
- Mirror position, window positions
- Battery voltage
- Climate: fan speed, temperature, AC status, seat heaters

**Vehicle Control:**
- Window control (up/down/position)
- Dome light control
- Driver seat heater control

**Development Tools:**
- WiFi AP + web dashboard (live vehicle data at http://192.168.4.1)
- Serial interface for manual CAN frame injection
- Python CAN debugging suite (`client/`)

### Python Client Tools

Located in `client/`:
- **can_reader.py** - CAN frame logger with filtering
- **can_sender.py** - Manual CAN frame transmission
- **nibble_monitor.py** - Nibble-level CAN analyzer for reverse engineering

## Project Structure

```
├── src/main.cpp              # Main application loop
├── include/config.h          # Feature toggles (DEBUG_MODE, ENABLE_WEBSERVER)
├── lib/
│   ├── CarControl/           # Vehicle state management & CAN parsing
│   ├── ClimateControl/       # Climate system state & control
│   ├── CANBus/               # MCP2515 driver wrapper
│   └── WebServer/            # WiFi AP + web dashboard
├── client/                   # Python CAN debugging tools
├── CAN_IDs.md                # Complete CAN protocol documentation
└── IMPLEMENTATION_STATUS.md  # Feature checklist
```

## Quick Start

### Build & Upload
```bash
platformio run          # Compile
platformio upload       # Flash to ESP8266
platformio device monitor  # Serial monitor (115200 baud)
```

### Web Dashboard (Optional)
1. Enable `#define ENABLE_WEBSERVER` in `include/config.h`
2. Upload firmware
3. Connect to WiFi: **CANDebugger** (password: `candebugger123`)
4. Browse to **http://192.168.4.1**

### Configuration
Edit `include/config.h`:
- `#define DEBUG_MODE` - Enable raw CAN frame output
- `#define ENABLE_WEBSERVER` - Enable WiFi AP + web interface

## Development

### Adding New CAN IDs
1. Document protocol in `CAN_IDs.md`
2. Add state fields to `CarControl::CarState` or `ClimateControl::ClimateState`
3. Add parser case in `parseCanFrame()`
4. Add getter/control methods

### Testing
- Serial injection: `xxx:yyyy...` (hex ID:data bytes)
- See `IMPLEMENTATION_STATUS.md` for test commands

## Documentation

- **CAN_IDs.md** - Complete CAN protocol reverse engineering notes
- **CLAUDE.md** - Project architecture & development guidelines
- **IMPLEMENTATION_STATUS.md** - Feature status & testing procedures

## License

Personal project - use at your own risk. Modifying vehicle electronics can void warranties and may be illegal in your jurisdiction.
