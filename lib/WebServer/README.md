# WebServer - WiFi Dashboard for CAN Debugger

## Overview
Provides a WiFi access point and web-based dashboard to monitor all vehicle data in real-time.

## Features
- **WiFi Access Point:** Creates standalone network (no router needed)
- **Live Dashboard:** Auto-refreshing HTML interface
- **All Vehicle Data:** Engine, climate, windows, doors, etc.
- **Minimal Integration:** Just 3 lines in main.cpp
- **Easy Toggle:** Disable completely with config.h

## Usage

### Enable/Disable
In `include/config.h`:
```cpp
#define ENABLE_WEBSERVER  // Comment out to disable
```

### Integration in main.cpp
```cpp
#ifdef ENABLE_WEBSERVER
#include "WebServer.hpp"
VehicleWebServer* webServer = VehicleWebServer::getInstance();
#endif

void setup() {
    // ... other init ...
    #ifdef ENABLE_WEBSERVER
    webServer->init(carControl, climateControl);
    #endif
}

void loop() {
    #ifdef ENABLE_WEBSERVER
    webServer->update();
    #endif
}
```

### Access the Dashboard
1. Upload firmware
2. Connect to WiFi: **CANDebugger** (password: candebugger123)
3. Open browser to: **http://192.168.4.1**

## Configuration
WiFi settings in `include/config.h`:
```cpp
#define WIFI_AP_SSID "CANDebugger"
#define WIFI_AP_PASSWORD "candebugger123"
#define WIFI_AP_CHANNEL 6
```

## Dashboard Sections

### Engine & Powertrain
- Engine status (ON/OFF)
- RPM
- Throttle position (% or KICKDOWN)
- Battery voltage

### Vehicle Status
- Braking (YES/NO)
- Parking brake (ON/OFF)
- Steering wheel angle
- Doors locked/open

### Climate Control
- Fan speed (0-7)
- Driver temperature (°C)
- Passenger temperature (°C)
- AC compressor status

### Windows
- All 4 windows (% open)

## API Endpoints

### GET /
Returns full HTML dashboard with JavaScript auto-refresh

### GET /data
Returns JSON with current vehicle state:
```json
{
  "engineRPM": 2000,
  "throttle": "45%",
  "steering": -15.5,
  "battery": 14.2,
  "engineRunning": "ON",
  "braking": "NO",
  "parkingBrake": "OFF",
  "doorLocked": "YES",
  "doorOpen": "NO",
  "fanSpeed": "3",
  "driverTemp": 22,
  "passengerTemp": 22,
  "acActive": "ON",
  "windowDF": 0,
  "windowPF": 0,
  "windowDR": 50,
  "windowPR": 50
}
```

## Memory Usage
- **RAM:** ~776 bytes additional (35.5% total)
- **Flash:** ~40KB additional (30.1% total)

## Implementation Notes
- Uses ESP8266WiFi and ESP8266WebServer libraries (built-in)
- Non-blocking: `update()` only calls `server.handleClient()`
- Static HTML embedded in flash (F() macro)
- JavaScript fetch API for live updates (500ms interval)
- Singleton pattern (same as CarControl/ClimateControl)

## Troubleshooting

**Can't connect to WiFi:**
- Wait 5-10 seconds after power-up for AP to start
- Check WiFi password (min 8 characters)
- Look for "CANDebugger" in WiFi networks list

**Dashboard doesn't update:**
- Check browser console for errors
- Ensure both CarControl and ClimateControl are initialized
- Verify CAN bus is receiving frames

**Compile errors:**
- Ensure ESP8266 platform is selected (not ESP32)
- Check that ESP8266WiFi library is available
