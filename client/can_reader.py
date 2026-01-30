#!/usr/bin/env python3
"""Simple CAN frame reader for ESP32 USB serial output.

Uses curses for a register-file style display where each CAN ID occupies
a fixed line and data updates in-place with change highlighting.

Supports blacklist files (from nibble_monitor.py) to ignore noisy nibbles
like counters and checksums.

Dependencies:
    pip install pyserial
    pip install windows-curses  # Windows only
"""

import argparse
import json
import re
import serial
import serial.tools.list_ports

try:
    import curses
    CURSES_AVAILABLE = True
except ImportError:
    CURSES_AVAILABLE = False


# Color pair constants
COLOR_NEW = 1      # Green - new ID or changed nibble
COLOR_NORMAL = 2   # White - unchanged data
COLOR_ID = 3       # Red - CAN ID label


def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def load_blacklist(filepath: str) -> dict:
    """Load nibble blacklist from JSON file.

    Expected format: {"CAN_ID": [nibble_indices], ...}
    Example: {"123": [14, 15], "456": [0, 1, 14, 15]}

    Returns dict with CAN IDs as keys and sets of nibble indices as values.
    """
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        # Convert lists to sets for faster lookup
        blacklist = {can_id: set(nibbles) for can_id, nibbles in data.items()}
        print(f"Loaded blacklist: {len(blacklist)} CAN IDs")
        for can_id, nibbles in blacklist.items():
            print(f"  {can_id}: nibbles {sorted(nibbles)}")
        return blacklist
    except FileNotFoundError:
        print(f"Warning: Blacklist file not found: {filepath}")
        return {}
    except json.JSONDecodeError as e:
        print(f"Warning: Invalid JSON in blacklist file: {e}")
        return {}


def parse_can_frame(line: str):
    """Parse a CAN frame line.

    Expected format: RX: 0x0a9 Data: 5e 47 b3 9f 2b b3 3f fb
    Returns (id_str, data_bytes) or (None, None) if parsing fails.
    """
    match = re.match(r'RX:\s*0x([0-9A-Fa-f]+)\s+Data:\s*((?:[0-9A-Fa-f]{2}\s*)+)', line)
    if not match:
        return None, None

    can_id = match.group(1).upper().zfill(3)
    data_hex = match.group(2).strip()
    data_bytes = data_hex.split()

    return can_id, data_bytes


def read_can_frames_curses(stdscr, port: str, baudrate: int, blacklist: dict = None):
    """Read and display CAN frames using curses register-file view.

    Args:
        stdscr: curses window
        port: Serial port name
        baudrate: Serial baud rate
        blacklist: Optional dict of {can_id: [nibble_indices]} to ignore for highlighting
    """
    if blacklist is None:
        blacklist = {}

    # Initialize colors
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(COLOR_NEW, curses.COLOR_GREEN, -1)
    curses.init_pair(COLOR_NORMAL, curses.COLOR_WHITE, -1)
    curses.init_pair(COLOR_ID, curses.COLOR_RED, -1)

    # Non-blocking input
    stdscr.nodelay(True)
    curses.curs_set(0)  # Hide cursor

    # Track each CAN ID's state
    # {id_str: {"data": [bytes], "changed_mask": [bool, ...], "screen_pos": int}}
    can_registers = {}
    next_screen_pos = 0  # Next available screen position for new CAN IDs

    header_rows = 3
    blacklist_info = f" | Blacklist: {len(blacklist)} IDs" if blacklist else ""

    try:
        with serial.Serial(port, baudrate, timeout=0.05) as ser:
            # Draw header
            stdscr.clear()
            stdscr.addstr(0, 0, f"CAN Register View - {port} @ {baudrate} baud",
                         curses.A_BOLD)
            stdscr.addstr(1, 0, f"Press 'q' to quit | Green = new/changed{blacklist_info}",
                         curses.A_DIM)
            stdscr.addstr(2, 0, "-" * 60)
            stdscr.refresh()

            buffer = ""

            while True:
                # Check for quit key
                try:
                    key = stdscr.getch()
                    if key == ord('q') or key == ord('Q'):
                        break
                except:
                    pass

                # Read serial data
                try:
                    raw = ser.read(256)
                    if raw:
                        buffer += raw.decode('utf-8', errors='ignore')

                        # Process complete lines
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()

                            if not line:
                                continue

                            can_id, data_bytes = parse_can_frame(line)
                            if can_id is None:
                                continue

                            # Ensure we have 8 bytes (pad if needed)
                            while len(data_bytes) < 8:
                                data_bytes.append("--")
                            data_bytes = data_bytes[:8]

                            if can_id not in can_registers:
                                # New ID - mark all nibbles as changed (green)
                                # But respect blacklist - don't highlight blacklisted nibbles
                                # 16 nibbles (2 per byte * 8 bytes)
                                blacklisted_nibbles = set(blacklist.get(can_id, []))
                                initial_mask = [i not in blacklisted_nibbles for i in range(16)]
                                can_registers[can_id] = {
                                    "data": data_bytes,
                                    "changed_mask": initial_mask,
                                    "is_new": True,
                                    "blacklisted": blacklisted_nibbles,
                                    "screen_pos": next_screen_pos
                                }
                                next_screen_pos += 1
                            else:
                                # Existing ID - compare nibble by nibble
                                reg = can_registers[can_id]
                                old_data = reg["data"]
                                blacklisted_nibbles = reg.get("blacklisted", set())
                                new_mask = []

                                for i, (old_byte, new_byte) in enumerate(zip(old_data, data_bytes)):
                                    # Compare high nibble (nibble index = i * 2)
                                    high_nibble_idx = i * 2
                                    old_high = old_byte[0] if len(old_byte) > 0 else '-'
                                    new_high = new_byte[0] if len(new_byte) > 0 else '-'
                                    # Only mark as changed if not blacklisted
                                    high_changed = (old_high != new_high) and (high_nibble_idx not in blacklisted_nibbles)
                                    new_mask.append(high_changed)

                                    # Compare low nibble (nibble index = i * 2 + 1)
                                    low_nibble_idx = i * 2 + 1
                                    old_low = old_byte[1] if len(old_byte) > 1 else '-'
                                    new_low = new_byte[1] if len(new_byte) > 1 else '-'
                                    # Only mark as changed if not blacklisted
                                    low_changed = (old_low != new_low) and (low_nibble_idx not in blacklisted_nibbles)
                                    new_mask.append(low_changed)

                                reg["data"] = data_bytes
                                reg["changed_mask"] = new_mask
                                reg["is_new"] = False

                            # Determine which IDs to redraw
                            max_y, max_x = stdscr.getmaxyx()

                            # Only redraw the current ID
                            ids_to_draw = [can_id]

                            for draw_id in ids_to_draw:
                                # Calculate row and column based on fixed screen position
                                # Display 3 CAN IDs per row
                                screen_pos = can_registers[draw_id]["screen_pos"]
                                row = header_rows + (screen_pos // 3)
                                column_idx = screen_pos % 3

                                # Column starting positions: 0, 40, 80
                                col_start = column_idx * 40
                                col_width = 38  # Width of each column section

                                # Check if row fits on screen
                                if row >= max_y - 1:
                                    continue

                                reg = can_registers[draw_id]

                                # Clear only the specific column section
                                for clear_col in range(col_start, min(col_start + col_width, max_x)):
                                    stdscr.addstr(row, clear_col, " ")

                                # Draw CAN ID in red
                                stdscr.addstr(row, col_start, f"{draw_id.lower()}:", curses.color_pair(COLOR_ID) | curses.A_BOLD)

                                # Draw each byte with nibble-level highlighting
                                col = col_start + 4  # Starting column after "XXX:"
                                for byte_idx, byte_val in enumerate(reg["data"]):
                                    high_nibble = byte_val[0] if len(byte_val) > 0 else '-'
                                    low_nibble = byte_val[1] if len(byte_val) > 1 else '-'

                                    high_changed = reg["changed_mask"][byte_idx * 2]
                                    low_changed = reg["changed_mask"][byte_idx * 2 + 1]

                                    # Draw high nibble
                                    high_attr = curses.color_pair(COLOR_NEW) | curses.A_BOLD if high_changed else curses.color_pair(COLOR_NORMAL)
                                    stdscr.addstr(row, col, high_nibble, high_attr)
                                    col += 1

                                    # Draw low nibble
                                    low_attr = curses.color_pair(COLOR_NEW) | curses.A_BOLD if low_changed else curses.color_pair(COLOR_NORMAL)
                                    stdscr.addstr(row, col, low_nibble, low_attr)
                                    col += 1

                                    # Space between bytes
                                    stdscr.addstr(row, col, " ")
                                    col += 1

                            stdscr.refresh()

                except serial.SerialException:
                    break

    except serial.SerialException as e:
        stdscr.addstr(header_rows, 0, f"Serial error: {e}")
        stdscr.refresh()
        stdscr.nodelay(False)
        stdscr.getch()


def read_can_frames_simple(port: str, baudrate: int):
    """Read and display CAN frames from serial port (simple mode).

    Expected format: RX: 0x0a9 Data: 5e 47 b3 9f 2b b3 3f fb
    """
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            print(f"Connected to {port} at {baudrate} baud")
            print("Reading CAN frames (Ctrl+C to stop)...\n")

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)

    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("\nStopped.")


def main():
    parser = argparse.ArgumentParser(
        description="Read CAN frames from ESP32 via USB serial",
        epilog="Requires: pip install pyserial windows-curses (Windows only)"
    )
    parser.add_argument("port", nargs="?", help="Serial port (e.g., COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--baudrate", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-l", "--list", action="store_true", help="List available serial ports")
    parser.add_argument("-s", "--simple", action="store_true", help="Use simple output mode (no curses)")
    parser.add_argument("--blacklist", type=str, help="JSON file with nibbles to ignore (from nibble_monitor.py)")

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    if not args.port:
        print("Error: No port specified. Use -l to list available ports.\n")
        parser.print_help()
        return

    # Load blacklist if provided
    blacklist = {}
    if args.blacklist:
        blacklist = load_blacklist(args.blacklist)
        print()  # Blank line after blacklist info

    if args.simple or not CURSES_AVAILABLE:
        if not CURSES_AVAILABLE:
            print("Note: curses not available, using simple mode.")
            print("Install with: pip install windows-curses\n")
        read_can_frames_simple(args.port, args.baudrate)
    else:
        curses.wrapper(lambda stdscr: read_can_frames_curses(stdscr, args.port, args.baudrate, blacklist))


if __name__ == "__main__":
    main()
