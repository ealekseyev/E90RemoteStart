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
import time

try:
    import curses
    CURSES_AVAILABLE = True
except ImportError:
    CURSES_AVAILABLE = False


# CAN IDs to highlight in purple (customize this list as needed)
PURPLE_IDS = [
    "0E2", "0E6", "0EA", "0EE", "0F2", "0F6", "0FA", "0FB",
    "0FC", "0FD", "1A0", "1A3", "1C6", "192", "1E2", "2A0"
]

# Color pair constants
COLOR_NEW = 1         # Green - new ID or changed nibble
COLOR_NORMAL = 2      # White - unchanged data
COLOR_ID = 3          # Red - CAN ID label
COLOR_PURPLE_ID = 4   # Purple - special CAN IDs


class InputSource:
    """Abstract base class for input sources."""
    def readline(self):
        """Read a complete line. Returns empty string on timeout/EOF."""
        raise NotImplementedError

    def read(self, size):
        """Read up to size bytes. Returns bytes."""
        raise NotImplementedError

    def close(self):
        """Close the input source."""
        pass


class SerialInputSource(InputSource):
    """Read from serial port, optionally logging to file."""
    def __init__(self, port, baudrate, log_file=None):
        self.ser = serial.Serial(port, baudrate, timeout=0.05)
        self.log_file = log_file
        self.log_handle = None
        if log_file:
            self.log_handle = open(log_file, 'w', encoding='utf-8')

    def readline(self):
        line_bytes = self.ser.readline()
        if self.log_handle and line_bytes:
            self.log_handle.write(line_bytes.decode('utf-8', errors='ignore'))
            self.log_handle.flush()
        return line_bytes.decode('utf-8', errors='ignore')

    def read(self, size):
        data = self.ser.read(size)
        if self.log_handle and data:
            self.log_handle.write(data.decode('utf-8', errors='ignore'))
            self.log_handle.flush()
        return data

    def send_frame(self, frame: str):
        """Send a CAN frame over serial."""
        self.ser.write(f"{frame}\n".encode('utf-8'))
        self.ser.flush()

    def close(self):
        if self.log_handle:
            self.log_handle.close()
        self.ser.close()


class FileInputSource(InputSource):
    """Read from log file for replay."""
    def __init__(self, filepath):
        self.file = open(filepath, 'r', encoding='utf-8')

    def readline(self):
        line = self.file.readline()
        return line

    def read(self, size):
        # Read from file and return as bytes
        text = self.file.read(size)
        if not text:
            return b''
        return text.encode('utf-8')

    def close(self):
        self.file.close()


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


def format_can_frame(can_id: str, data_bytes: list) -> str:
    """Format a CAN frame for transmission.

    Args:
        can_id: 3-character hex ID (e.g., "012", "0a9")
        data_bytes: List of hex byte strings (e.g., ["12", "34", "ff"])

    Returns:
        Formatted frame string: XXX:YYYYYYYYYYYYYYYY
    """
    can_id = can_id.lower().zfill(3)[-3:]
    while len(data_bytes) < 8:
        data_bytes.append("00")
    data_bytes = data_bytes[:8]
    payload = "".join(b.lower().zfill(2) for b in data_bytes)
    return f"{can_id}:{payload}"


def parse_send_command(command: str):
    """Parse a send command.

    Format: XXX:YY YY YY... [repeat XXX]
    Where XXX is hex CAN ID, YY are hex bytes, and optional repeat interval in ms

    Returns:
        (can_id, data_bytes, repeat_ms) or None if parsing fails
    """
    command = command.strip()

    if not command:
        return None

    # Match: XXX:YY YY YY... [repeat XXX]
    match = re.match(
        r'([0-9a-f]{1,3}):(.+?)(?:\s+repeat\s+(\d+))?$',
        command,
        re.IGNORECASE
    )

    if not match:
        return None

    can_id = match.group(1)
    data_str = match.group(2).strip()
    repeat_ms = int(match.group(3)) if match.group(3) else None

    # Parse data bytes (space-separated)
    data_bytes = data_str.split()

    # Validate hex bytes
    for byte in data_bytes:
        if not re.match(r'^[0-9a-f]{1,2}$', byte, re.IGNORECASE):
            return None

    return can_id, data_bytes, repeat_ms


def read_can_frames_curses(stdscr, input_source: InputSource, source_name: str, blacklist: dict = None):
    """Read and display CAN frames using curses register-file view.

    Args:
        stdscr: curses window
        input_source: InputSource to read from
        source_name: Display name for the source
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
    curses.init_pair(COLOR_PURPLE_ID, curses.COLOR_MAGENTA, -1)

    # Non-blocking input
    stdscr.nodelay(True)
    curses.curs_set(2)  # Show cursor (2 = very visible, reduces flicker)

    # Track each CAN ID's state
    # {id_str: {"data": [bytes], "changed_mask": [bool, ...], "screen_pos": int}}
    can_registers = {}
    next_screen_pos = 0  # Next available screen position for new CAN IDs

    header_rows = 3
    blacklist_info = f" | Blacklist: {len(blacklist)} IDs" if blacklist else ""

    # Check if we can send commands (only for serial sources)
    can_send = hasattr(input_source, 'send_frame')
    command_input = ""
    cursor_pos = 0
    feedback_msg = ""
    feedback_time = 0

    # Repeat mode state
    repeat_active = False
    repeat_frame = ""
    repeat_interval = 0
    repeat_last_send = 0
    repeat_count = 0

    try:
        # Draw header
        stdscr.clear()
        stdscr.addstr(0, 0, f"CAN Register View - {source_name}",
                     curses.A_BOLD)
        help_text = f"Press 'q' to quit | Green = new/changed{blacklist_info}"
        if can_send:
            help_text += " | Format: XXX:YY YY... [repeat MS]"
        stdscr.addstr(1, 0, help_text, curses.A_DIM)
        stdscr.addstr(2, 0, "-" * 60)
        stdscr.refresh()

        buffer = ""

        while True:
            # Handle repeat mode
            if repeat_active and can_send:
                current_time = time.time()
                if current_time - repeat_last_send >= repeat_interval / 1000.0:
                    try:
                        input_source.send_frame(repeat_frame)
                        repeat_count += 1
                        repeat_last_send = current_time
                        feedback_msg = f"Repeating: {repeat_frame} (sent {repeat_count}x)"
                        feedback_time = current_time
                    except Exception as e:
                        feedback_msg = f"Repeat error: {e}"
                        feedback_time = current_time
                        repeat_active = False

            # Handle keyboard input
            try:
                key = stdscr.getch()
                if key != -1:
                    # Clear old feedback after 3 seconds (if not repeating)
                    if feedback_msg and not repeat_active and time.time() - feedback_time > 3:
                        feedback_msg = ""

                    if key == ord('q') or key == ord('Q'):
                        if not command_input and not repeat_active:  # Only quit if not typing a command
                            break
                        elif repeat_active:
                            # Stop repeat mode
                            repeat_active = False
                            feedback_msg = f"Stopped repeating after {repeat_count} sends"
                            feedback_time = time.time()
                        else:
                            # Add to command
                            command_input = command_input[:cursor_pos] + chr(key) + command_input[cursor_pos:]
                            cursor_pos += 1
                    elif can_send and key == ord('\n'):  # Enter key
                        if repeat_active:
                            # Stop repeat mode
                            repeat_active = False
                            feedback_msg = f"Stopped repeating after {repeat_count} sends"
                            feedback_time = time.time()
                        elif command_input.strip():
                            # Parse and send command
                            result = parse_send_command(command_input)
                            if result:
                                can_id, data_bytes, repeat_ms = result
                                frame = format_can_frame(can_id, data_bytes)

                                if repeat_ms:
                                    # Start repeat mode
                                    repeat_active = True
                                    repeat_frame = frame
                                    repeat_interval = repeat_ms
                                    repeat_last_send = 0  # Send immediately
                                    repeat_count = 0
                                    feedback_msg = f"Started repeating: {frame} every {repeat_ms}ms (press Enter/Esc to stop)"
                                    feedback_time = time.time()
                                else:
                                    # Single send
                                    try:
                                        input_source.send_frame(frame)
                                        feedback_msg = f"Sent: {frame}"
                                        feedback_time = time.time()
                                    except Exception as e:
                                        feedback_msg = f"Error: {e}"
                                        feedback_time = time.time()
                            else:
                                feedback_msg = "Invalid format. Use: XXX:YY YY... [repeat MS]"
                                feedback_time = time.time()
                            command_input = ""
                            cursor_pos = 0
                    elif can_send and key == 27:  # Escape key
                        if repeat_active:
                            # Stop repeat mode
                            repeat_active = False
                            feedback_msg = f"Stopped repeating after {repeat_count} sends"
                            feedback_time = time.time()
                        else:
                            command_input = ""
                            cursor_pos = 0
                            feedback_msg = ""
                    elif can_send and (key == curses.KEY_BACKSPACE or key == 127 or key == 8):
                        if cursor_pos > 0:
                            command_input = command_input[:cursor_pos-1] + command_input[cursor_pos:]
                            cursor_pos -= 1
                    elif can_send and key == curses.KEY_DC:  # Delete key
                        if cursor_pos < len(command_input):
                            command_input = command_input[:cursor_pos] + command_input[cursor_pos+1:]
                    elif can_send and key == curses.KEY_LEFT:
                        if cursor_pos > 0:
                            cursor_pos -= 1
                    elif can_send and key == curses.KEY_RIGHT:
                        if cursor_pos < len(command_input):
                            cursor_pos += 1
                    elif can_send and key == curses.KEY_HOME:
                        cursor_pos = 0
                    elif can_send and key == curses.KEY_END:
                        cursor_pos = len(command_input)
                    elif can_send and key == 22:  # Ctrl+V (paste)
                        # Note: Paste from clipboard is terminal-dependent
                        # This will work if terminal sends pasted text as normal input
                        pass
                    elif can_send and 32 <= key <= 126:  # Printable characters
                        command_input = command_input[:cursor_pos] + chr(key) + command_input[cursor_pos:]
                        cursor_pos += 1
            except:
                pass

            # Read data
            try:
                raw = input_source.read(256)
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

                            # Draw CAN ID (purple for special IDs, red for others)
                            id_color = COLOR_PURPLE_ID if draw_id in PURPLE_IDS else COLOR_ID
                            stdscr.addstr(row, col_start, f"{draw_id.lower()}:", curses.color_pair(id_color) | curses.A_BOLD)

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

            except Exception:
                pass

            # Draw command input area at bottom if sending is enabled (only redraw when needed)
            if can_send:
                max_y, max_x = stdscr.getmaxyx()
                cmd_row = max_y - 2
                if cmd_row > header_rows:
                    # Draw separator
                    try:
                        stdscr.addstr(cmd_row - 1, 0, "-" * min(60, max_x - 1))
                    except:
                        pass
                    # Draw command prompt
                    try:
                        stdscr.addstr(cmd_row, 0, " " * (max_x - 1))  # Clear line
                        prompt = "Send> "
                        stdscr.addstr(cmd_row, 0, prompt)

                        # Draw command input
                        display_input = command_input[:max_x - len(prompt) - 1]
                        stdscr.addstr(cmd_row, len(prompt), display_input)
                    except:
                        pass
                    # Draw feedback message
                    if feedback_msg:
                        try:
                            stdscr.addstr(cmd_row + 1, 0, " " * (max_x - 1))  # Clear line
                            # Highlight repeat messages in yellow
                            attr = curses.A_DIM
                            if repeat_active:
                                attr = curses.color_pair(COLOR_NEW)
                            stdscr.addstr(cmd_row + 1, 0, feedback_msg[:max_x - 1], attr)
                        except:
                            pass

            # Position cursor at the end (do this ONCE before refresh to reduce flicker)
            if can_send:
                max_y, max_x = stdscr.getmaxyx()
                cmd_row = max_y - 2
                if cmd_row > header_rows:
                    prompt = "Send> "
                    display_input = command_input[:max_x - len(prompt) - 1]
                    cursor_x = len(prompt) + min(cursor_pos, len(display_input))
                    try:
                        if cursor_x < max_x - 1:
                            stdscr.move(cmd_row, cursor_x)
                    except:
                        pass

            stdscr.refresh()

    except Exception as e:
        stdscr.addstr(header_rows, 0, f"Error: {e}")
        stdscr.refresh()
        stdscr.nodelay(False)
        stdscr.getch()
    finally:
        input_source.close()


def read_can_frames_simple(input_source: InputSource, source_name: str):
    """Read and display CAN frames (simple mode).

    Expected format: RX: 0x0a9 Data: 5e 47 b3 9f 2b b3 3f fb
    """
    try:
        print(f"Reading from {source_name}")
        print("Reading CAN frames (Ctrl+C to stop)...\n")

        while True:
            line = input_source.readline().strip()
            if line:
                print(line)

    except Exception as e:
        print(f"Error: {e}")
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        input_source.close()


def main():
    parser = argparse.ArgumentParser(
        description="Read CAN frames from ESP32 via USB serial",
        epilog="Requires: pip install pyserial windows-curses (Windows only)"
    )
    parser.add_argument("port", nargs="?", help="Serial port (COM3, /dev/ttyUSB0) - not needed with --replay")
    parser.add_argument("-b", "--baudrate", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-l", "--list", action="store_true", help="List available serial ports")
    parser.add_argument("-s", "--simple", action="store_true", help="Use simple output mode (no curses)")
    parser.add_argument("--blacklist", type=str, help="JSON file with nibbles to ignore (from nibble_monitor.py)")
    parser.add_argument("--log", type=str, metavar="FILE", help="Log raw serial output to file")
    parser.add_argument("--replay", type=str, metavar="FILE", help="Replay from log file instead of serial")

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    # Validate arguments and create input source
    if args.replay:
        # Replay mode - no port needed
        input_source = FileInputSource(args.replay)
        source_name = f"Replay: {args.replay}"
        if args.log:
            print("Warning: --log ignored in replay mode")
    else:
        # Serial mode - port required
        if not args.port:
            print("Error: No port specified. Use -l to list available ports or --replay <file>.\n")
            parser.print_help()
            return

        input_source = SerialInputSource(args.port, args.baudrate, args.log)
        source_name = f"{args.port} @ {args.baudrate} baud"
        if args.log:
            print(f"Logging to: {args.log}")

    # Load blacklist if provided
    blacklist = {}
    if args.blacklist:
        blacklist = load_blacklist(args.blacklist)
        print()  # Blank line after blacklist info

    if args.simple or not CURSES_AVAILABLE:
        if not CURSES_AVAILABLE:
            print("Note: curses not available, using simple mode.")
            print("Install with: pip install windows-curses\n")
        read_can_frames_simple(input_source, source_name)
    else:
        curses.wrapper(lambda stdscr: read_can_frames_curses(stdscr, input_source, source_name, blacklist))


if __name__ == "__main__":
    main()
