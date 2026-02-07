# BMW E90 K-CAN Protocol Documentation

## Engine & Powertrain (READ-ONLY)

### 0x0AA - Engine RPM & Throttle Position
**Access:** Read-only
**Update Rate:** ~10-20ms (high frequency)

| Byte | Description | Calculation/Values |
|------|-------------|-------------------|
| 2-3 | Throttle position (raw) | `raw = (byte3 << 8) | byte2`<br>Range: 255 (foot off) to 65064 (foot flat) |
| 4-5 | Engine RPM | `raw = (byte5 << 8) | byte4`<br>`rpm = raw / 4` |
| 6 | Kickdown indicator | `0xB4` = Kickdown pressed |

**Scaled Throttle Output:**
- The `getThrottlePosition()` method returns a scaled value (0-255):
  - **0-254**: Normal throttle, scaled from raw range
    - 0 = Foot completely off
    - 254 = Foot flat to floor (no kickdown)
  - **255**: Kickdown pressed (special state)

**Usage:**
```cpp
uint16_t rpm = carControl->getEngineRPM();
uint8_t throttle = carControl->getThrottlePosition();

// Check throttle state
if (throttle == 0) {
    // Foot off accelerator
} else if (throttle == 255) {
    // Kickdown pressed
} else {
    // Normal throttle: 1-254
    // Can use as percentage: (throttle * 100) / 254
}
```

**Examples:**
| Raw Value | Byte 6 | Output | Description |
|-----------|--------|--------|-------------|
| 255 | Any | 0 | Foot off accelerator |
| 32000 | != 0xB4 | ~127 | ~50% throttle |
| 65064 | != 0xB4 | 254 | Foot flat (no kickdown) |
| Any | 0xB4 | 255 | Kickdown pressed |

### 0x130 - Key Position State
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Key position | 0x00 = Engine off (key removed)<br>0x40 = Key being inserted<br>0x41 = Key position 1 (accessories)<br>0x45 = Key position 2 (engine running)<br>0x55 = Engine cranking/auto start-stop |

**Implementation Notes:**
- Engine running detection combines key state + RPM > 400
- Falls back to 0x3B4 on vehicles without 0x130
- Cranking state (0x55) detected explicitly via `isEngineCranking()`

**Usage:**
```cpp
KeyState keyState = carControl->getKeyState();
bool running = carControl->isEngineRunning();       // true when key=pos2/cranking AND RPM>400
bool cranking = carControl->isEngineCranking();     // true when key=cranking AND RPM<400
IgnitionStatus status = carControl->getIgnitionStatus();
```

### 0x304 - Gear Position
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Gear position | 0xE3 = Park (P)<br>0xC2 = Reverse (R)<br>0xD1 = Neutral (N)<br>0xC7 = Drive (D) |

**Usage:**
```cpp
GearPosition gear = carControl->getGearPosition();

switch (gear) {
    case GEAR_PARK:
        // Vehicle in park
        break;
    case GEAR_REVERSE:
        // Vehicle in reverse
        break;
    case GEAR_NEUTRAL:
        // Vehicle in neutral
        break;
    case GEAR_DRIVE:
        // Vehicle in drive
        break;
    case GEAR_UNKNOWN:
        // Unknown gear position
        break;
}
```

**Examples:**
- `304:E3 FF ...` - Park
- `304:C2 FF ...` - Reverse
- `304:D1 FF ...` - Neutral
- `304:C7 FF ...` - Drive

## Window Position Status (READ-ONLY)

### 0x3B6 - Driver Front Window
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Analog position | `0x00` = fully up<br>`0x50+` = fully down |
| 1 | Discrete position | `0xFC` = fully up<br>`0xFD` = ~80% down<br>`0xFE` = fully down |

### 0x3B7 - Driver Rear Window
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Analog position | `0x00` = fully up<br>`0x50+` = fully down |
| 1 | Discrete position | `0xF0` = fully up<br>`0xF1` = ~80% down<br>`0xF2` = fully down |

### 0x3B8 - Passenger Front Window
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Analog position | `0x00` = fully up<br>`0x50+` = fully down |
| 1 | Discrete position | `0xFC` = fully up<br>`0xFD` = ~80% down<br>`0xFE` = fully down |

### 0x3B9 - Passenger Rear Window
**Access:** Read-only
**Update Rate:** ~100ms

| Byte | Description | Values |
|------|-------------|--------|
| 0 | Analog position | `0x00` = fully up<br>`0x50+` = fully down |
| 1 | Discrete position | `0xF0` = fully up<br>`0xF1` = ~80% down<br>`0xF2` = fully down |

### 0x3B4 - Battery Voltage & Engine Status
**Access:** Read-only
**Update Rate:** ~1000ms

| Byte | Description | Calculation |
|------|-------------|-------------|
| 0-1 | Battery voltage | `raw = (byte1 << 8) \| byte0`<br>`voltage = (raw - 0xF000) / 68.0` |
| 2 | Engine flag | `0x00` = on<br>`0x09` = not running |

**Ignition Status Logic:**
The ignition status combines the engine flag from 0x3B4 with RPM from 0x0AA:

- **OFF**: Engine flag is off (byte 2 != 0x00)
- **SECOND**: Engine flag on (byte 2 == 0x00) but RPM < 400
- **RUNNING**: Engine flag on AND RPM >= 400

**Engine Running:**
Returns true only when ignition is RUNNING (flag on + RPM > 400).

**Usage:**
```cpp
bool running = carControl->isEngineRunning();  // true only if RPM > 400
IgnitionStatus ignition = carControl->getIgnitionStatus();

switch (ignition) {
    case IGNITION_OFF:
        // Ignition off
        break;
    case IGNITION_SECOND:
        // Ignition in position 2, engine cranking or idling < 400 RPM
        break;
    case IGNITION_RUNNING:
        // Engine fully running
        break;
}
```

## Window Control (WRITE)

### 0x0FA - Window Control Command
**Access:** Write
**Format:** `[byte0] [byte1] [0xFF]`

| Byte | Description | Bits |
|------|-------------|------|
| 0 | Front windows | Base: `0xC0`<br>Bit 1 (`0x02`): Left/Driver DOWN<br>Bit 2 (`0x04`): Left/Driver UP<br>Bit 4 (`0x10`): Right/Passenger DOWN<br>Bit 5 (`0x20`): Right/Passenger UP |
| 1 | Rear windows | Same bit mapping as byte 0 |
| 2 | Fixed | Always `0xFF` |

**Usage:**
```cpp
// Single window
carControl->setWindow(DRIVER_FRONT, WINDOW_ROLL_DOWN);

// Multiple windows
carControl->setWindow(DRIVER_FRONT | PASSENGER_FRONT, WINDOW_ROLL_UP);

// All windows
carControl->setWindow(DRIVER_FRONT | PASSENGER_FRONT | DRIVER_REAR | PASSENGER_REAR, WINDOW_ROLL_DOWN);
```

**Window Flags:**
- `DRIVER_FRONT` = `0x01`
- `PASSENGER_FRONT` = `0x02`
- `DRIVER_REAR` = `0x04`
- `PASSENGER_REAR` = `0x08`

## Reading Window Position

```cpp
uint8_t pos = carControl->getWindowPosition(DRIVER_FRONT);
// Returns 0-255 (scaled from CAN 0x00-0x50)
// 0 = fully up, 255 = fully down
```

**Note:** Window positions are automatically scaled from the CAN bus range (0x00-0x50) to 0-255 for easier use.

## Climate Control (READ-ONLY)

### 0x2E6 - Fan Speed and Driver Temperature
**Access:** Read-only
**Update Rate:** ~100ms
**DLC:** 8 bytes

| Byte | Bits | Description | Values |
|------|------|-------------|--------|
| 0-4  | -    | Unknown | - |
| 5    | 0-2  | Fan speed | 0-7 (0=off, 7=max) |
| 6    | -    | Unknown | - |
| 7    | -    | Driver temp | 0x20-0x38 (17-28°C) |

**Temperature Conversion:**
```cpp
// Raw range: 0x20-0x38 (24 steps)
// Temperature range: 17-28°C (11 degrees)
celsius = 17 + ((rawValue - 0x20) * 11) / 24
```

**Usage:**
```cpp
uint8_t fanSpeed = climateControl->getFanSpeed();  // 0-7
int8_t driverTemp = climateControl->getDriverTemp();  // 17-28°C
```

**Examples:**
- `2e6:00 64 00 00 00 01 3F 20` - Fan speed 1, driver temp 17°C
- `2e6:00 64 00 00 00 07 3F 38` - Fan speed 7, driver temp 28°C

### 0x2EA - Passenger Temperature
**Access:** Read-only
**Update Rate:** ~100ms
**DLC:** 8 bytes

| Byte | Bits | Description | Values |
|------|------|-------------|--------|
| 0-6  | -    | Unknown | Usually 0xFF |
| 7    | -    | Passenger temp | 0x20-0x38 (17-28°C) |

**Temperature Conversion:**
```cpp
// Raw range: 0x20-0x38 (24 steps)
// Temperature range: 17-28°C (11 degrees)
celsius = 17 + ((rawValue - 0x20) * 11) / 24
```

**Usage:**
```cpp
int8_t passengerTemp = climateControl->getPassengerTemp();  // 17-28°C
```

**Examples:**
- `2ea:FF FF FF FF FF FF FF 20` - Passenger temp 17°C
- `2ea:FF FF FF FF FF FF FF 38` - Passenger temp 28°C

### 0x242 - AC Compressor Status
**Access:** Read-only
**Update Rate:** ~500ms
**DLC:** Variable

| Byte | Bits | Description | Values |
|------|------|-------------|--------|
| 0    | 0    | AC active | 0=off, 1=on |
| 0    | 1-7  | Other flags | - |
| 1-4  | -    | Unknown | - |

**Usage:**
```cpp
bool acOn = climateControl->isACActive();  // true if compressor running
```

**Examples:**
- `242:10 F1 FC FF FF` - AC off (bit 0 = 0)
- `242:11 F1 FC FF FF` - AC on (bit 0 = 1)

### 0x1E7 - Driver Seat Heater Control
**Access:** Write
**Update Rate:** Command (timed sequence)
**DLC:** 1 byte

| Byte | Description | Values |
|------|-------------|--------|
| 0    | Heater command | 0xD0 = Button press<br>0xC0 = Button release |

**Control Pattern:**
The seat heater cycles through levels: OFF → LOW → MEDIUM → HIGH → OFF
Each button press advances to the next level.

Multi-click control sequence (automatic):
1. Send press frame (0xD0)
2. Wait for level change on CAN 0x232 (or timeout 200ms)
3. Send release frame (0xC0)
4. Pause 100ms
5. Repeat until target level reached

**Usage:**
```cpp
climateControl->setDriverSeatHeater(0);  // Off
climateControl->setDriverSeatHeater(1);  // Low (1 click from off)
climateControl->setDriverSeatHeater(2);  // Medium (2 clicks from off)
climateControl->setDriverSeatHeater(3);  // High (3 clicks from off)
```

Function automatically calculates shortest path (e.g., HIGH→LOW = 2 clicks forward).

**Examples:**
- `1e7:d0` - Press button
- `1e7:c0` - Release button

### 0x232 - Seat Heater Status
**Access:** Read-only
**Update Rate:** ~100ms
**DLC:** 3 bytes

| Byte | Bits | Description | Values |
|------|------|-------------|--------|
| 0    | 4-7  | Driver heater level | 0=off, 1=low, 2=medium, 3=high |
| 0    | 0-3  | Passenger heater level | 0=off, 1=low, 2=medium, 3=high |
| 1-2  | -    | Fixed | 0x40 0xF0 |

**Nibble Encoding:**
- **Left nibble** (upper 4 bits): Driver side
- **Right nibble** (lower 4 bits): Passenger side

**Usage:**
```cpp
uint8_t driverLevel = climateControl->getDriverSeatHeaterLevel();    // 0-3
uint8_t passengerLevel = climateControl->getPassengerSeatHeaterLevel(); // 0-3
```

**Examples:**
- `232:00 40 F0` - Both heaters off
- `232:10 40 F0` - Driver low, passenger off
- `232:21 40 F0` - Driver medium, passenger low
- `232:32 40 F0` - Driver high, passenger medium
- `232:33 40 F0` - Both heaters on high
