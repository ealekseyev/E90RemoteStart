#!/usr/bin/env python3
"""CAN frame sender with interactive terminal.

Sends CAN frames over serial in the format: XXX:YYYYYYYYYYYYYYYY
Where XXX is the 3-digit hex ID and Y's are 16 nibbles (8 bytes) of payload.

Usage:
    send <ID> data <byte1> <byte2> ... [repeat <ms>]

Examples:
    send 012 data 12 34 56 78 91 01 23 34
    send 0a9 data ff 00 repeat 100
    send 123 data 01 02 03 04 05 06 07 08 repeat 50

Dependencies:
    pip install pyserial
"""

import argparse
import re
import sys
import time
import serial
import serial.tools.list_ports


def turn_right(ser):
    f1 = format_frame('1ee', ['04', 'ff'])
    f2 = format_frame('21a', ['00', '10', 'f7'])
    f3 = format_frame('1f6', ['91', 'f1'])

    send_frame(ser, f1)
    send_frame(ser, f2)
    send_frame(ser, format_frame('1f6', ['91', 'f2']))
    time.sleep(0.65)
    while True:
        send_frame(ser, f1)
        send_frame(ser, f2)
        send_frame(ser, f3)
        time.sleep(0.65)

def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def format_frame(can_id: str, data_bytes: list) -> str:
    """Format a CAN frame for transmission.

    Args:
        can_id: 3-character hex ID (e.g., "012", "0a9")
        data_bytes: List of hex byte strings (e.g., ["12", "34", "ff"])

    Returns:
        Formatted frame string: XXX:YYYYYYYYYYYYYYYY
    """
    # Ensure ID is 3 characters, lowercase
    can_id = can_id.lower().zfill(3)[-3:]

    # Pad data to 8 bytes with zeros
    while len(data_bytes) < 8:
        data_bytes.append("00")
    data_bytes = data_bytes[:8]

    # Build payload string (16 nibbles)
    payload = "".join(b.lower().zfill(2) for b in data_bytes)

    return f"{can_id}:{payload}"


def parse_send_command(command: str):
    """Parse a send command.

    Format: send <ID> data <bytes...> [repeat <ms>]

    Returns:
        (can_id, data_bytes, repeat_ms) or None if parsing fails
    """
    command = command.strip().lower()

    # Match: send <id> data <bytes> [repeat <ms>]
    match = re.match(
        r'send\s+([0-9a-f]{1,3})\s+data\s+((?:[0-9a-f]{1,2}\s*)+?)(?:\s+repeat\s+(\d+))?$',
        command,
        re.IGNORECASE
    )

    if not match:
        return None

    can_id = match.group(1)
    data_str = match.group(2).strip()
    repeat_ms = int(match.group(3)) if match.group(3) else None

    # Parse data bytes
    data_bytes = data_str.split()

    return can_id, data_bytes, repeat_ms


def send_frame(ser: serial.Serial, frame: str):
    """Send a frame over serial with newline."""
    ser.write(f"{frame}\n".encode('utf-8'))
    ser.flush()


def run_terminal(port: str, baudrate: int):
    """Run the interactive terminal."""
    try:
        ser = serial.Serial(port, baudrate, timeout=0.1)
        print(f"Connected to {port} at {baudrate} baud")
        print("CAN Frame Sender - Type 'help' for commands, 'quit' to exit\n")
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        return

    try:
        while True:
            try:
                command = input("> ").strip()
            except EOFError:
                break

            if not command:
                continue

            cmd_lower = command.lower()

            if cmd_lower in ('quit', 'exit', 'q'):
                break

            if cmd_lower == 'help':
                print_help()
                continue

            if cmd_lower == 'ports':
                list_ports()
                continue

            # Parse send command
            result = parse_send_command(command)
            if result is None:
                print("Invalid command. Type 'help' for usage.")
                continue

            can_id, data_bytes, repeat_ms = result
            frame = format_frame(can_id, data_bytes)

            if repeat_ms is not None:
                # Repeating send - blocking until Ctrl+C
                print(f"Sending {frame} every {repeat_ms}ms (Ctrl+C to stop)...")
                try:
                    count = 0
                    while True:
                        send_frame(ser, frame)
                        count += 1
                        sys.stdout.write(f"\rSent: {count}")
                        sys.stdout.flush()
                        time.sleep(repeat_ms / 1000.0)
                except KeyboardInterrupt:
                    print(f"\nStopped. Total sent: {count}")
            else:
                # Single send
                send_frame(ser, frame)
                print(f"Sent: {frame}")

    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        ser.close()
        print("Connection closed.")


def print_help():
    """Print help message."""
    print("""
Commands:
  send <ID> data <bytes...> [repeat <ms>]
      Send a CAN frame with the specified ID and data bytes.
      ID: 1-3 hex digits (e.g., 012, 0a9, 1ff)
      bytes: Space-separated hex bytes (e.g., 12 34 ff 00)
      repeat: Optional, sends every <ms> milliseconds until Ctrl+C

  Examples:
      send 012 data 12 34 56 78 91 01 23 34
      send 0a9 data ff 00
      send 123 data 01 02 03 04 05 06 07 08 repeat 100
      send 1a0 data 00 80 00 b0 ff 0f 60 42 repeat 50

  Frame format sent: XXX:YYYYYYYYYYYYYYYY
      XXX = 3-digit hex ID
      Y = 16 nibbles (8 bytes) of payload, zero-padded

  ports    - List available serial ports
  help     - Show this help message
  quit     - Exit the program
""")


def main():
    # ser = serial.Serial('COM3', 115200, timeout=0.1)
    # time.sleep(2)
    # turn_right(ser)
    #exit()
    parser = argparse.ArgumentParser(
        description="Send CAN frames over serial",
        epilog="Requires: pip install pyserial"
    )
    parser.add_argument("port", nargs="?", help="Serial port (e.g., COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--baudrate", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("-l", "--list", action="store_true",
                        help="List available serial ports")

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    if not args.port:
        print("Error: No port specified. Use -l to list available ports.\n")
        parser.print_help()
        return

    run_terminal(args.port, args.baudrate)


if __name__ == "__main__":
    main()
