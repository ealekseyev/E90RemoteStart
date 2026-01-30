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
| 2 | Engine status | `0x00` = running<br>`0x09` = not running |

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
