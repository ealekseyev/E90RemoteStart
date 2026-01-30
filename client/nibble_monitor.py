#!/usr/bin/env python3
"""CAN nibble change monitor.

Monitors CAN bus traffic for a specified duration and tracks which nibbles
change for each CAN ID. Useful for identifying counters and checksums that
should be blacklisted in CAN analyzer tools.

Dependencies:
    pip install pyserial
"""

import argparse
import json
import re
import sys
import time
import serial
import serial.tools.list_ports
from collections import defaultdict


def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


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


def monitor_nibble_changes(port: str, baudrate: int, duration: float, verbose: bool = False):
    """Monitor CAN bus and track nibble changes for each ID.

    Returns a dictionary with statistics for each CAN ID.
    """
    # Track state for each CAN ID
    # {can_id: {
    #     "last_data": [bytes],
    #     "change_counts": [int] * 16,  # how many times each nibble changed
    #     "frame_count": int,
    #     "first_seen": float,
    #     "last_seen": float
    # }}
    can_stats = {}

    start_time = time.time()
    end_time = start_time + duration
    total_frames = 0

    print(f"Monitoring CAN bus on {port} @ {baudrate} baud for {duration} seconds...")
    print("Press Ctrl+C to stop early.\n")

    try:
        with serial.Serial(port, baudrate, timeout=0.1) as ser:
            buffer = ""

            while time.time() < end_time:
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

                            # Ensure we have 8 bytes
                            while len(data_bytes) < 8:
                                data_bytes.append("00")
                            data_bytes = data_bytes[:8]

                            current_time = time.time()
                            total_frames += 1

                            if can_id not in can_stats:
                                # First occurrence of this ID
                                can_stats[can_id] = {
                                    "last_data": data_bytes,
                                    "change_counts": [0] * 16,
                                    "frame_count": 1,
                                    "first_seen": current_time,
                                    "last_seen": current_time
                                }
                                if verbose:
                                    print(f"[+] New CAN ID: {can_id}")
                            else:
                                # Compare with previous data
                                stats = can_stats[can_id]
                                old_data = stats["last_data"]

                                for byte_idx in range(8):
                                    old_byte = old_data[byte_idx].upper()
                                    new_byte = data_bytes[byte_idx].upper()

                                    # Compare high nibble
                                    old_high = old_byte[0] if len(old_byte) > 0 else '0'
                                    new_high = new_byte[0] if len(new_byte) > 0 else '0'
                                    if old_high != new_high:
                                        stats["change_counts"][byte_idx * 2] += 1

                                    # Compare low nibble
                                    old_low = old_byte[1] if len(old_byte) > 1 else '0'
                                    new_low = new_byte[1] if len(new_byte) > 1 else '0'
                                    if old_low != new_low:
                                        stats["change_counts"][byte_idx * 2 + 1] += 1

                                stats["last_data"] = data_bytes
                                stats["frame_count"] += 1
                                stats["last_seen"] = current_time

                    # Progress indicator
                    elapsed = time.time() - start_time
                    remaining = duration - elapsed
                    if verbose:
                        sys.stdout.write(f"\r[{elapsed:.1f}s / {duration}s] IDs: {len(can_stats)}, Frames: {total_frames}   ")
                        sys.stdout.flush()

                except serial.SerialException as e:
                    print(f"\nSerial error: {e}")
                    break

    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        return None
    except KeyboardInterrupt:
        print("\n\nMonitoring stopped early by user.")

    actual_duration = time.time() - start_time
    print(f"\n\nMonitoring complete. Duration: {actual_duration:.1f}s, Total frames: {total_frames}")

    return can_stats, actual_duration, total_frames


def analyze_results(can_stats: dict, duration: float, total_frames: int, threshold: float = 0.1):
    """Analyze monitoring results and identify frequently changing nibbles.

    Args:
        can_stats: Dictionary of CAN ID statistics
        duration: Actual monitoring duration
        total_frames: Total number of frames received
        threshold: Fraction of frames where nibble must change to be considered "frequent" (default 10%)

    Returns:
        Analysis results dictionary
    """
    results = {
        "monitor_duration_seconds": round(duration, 2),
        "total_frames": total_frames,
        "threshold_percent": threshold * 100,
        "can_ids": {}
    }

    for can_id in sorted(can_stats.keys()):
        stats = can_stats[can_id]
        frame_count = stats["frame_count"]
        change_counts = stats["change_counts"]

        # Calculate change rate for each nibble
        change_rates = [count / max(frame_count - 1, 1) for count in change_counts]

        # Identify frequently changing nibbles (likely counters/checksums)
        frequent_changers = [i for i, rate in enumerate(change_rates) if rate >= threshold]

        # Identify stable nibbles (good for state detection)
        stable_nibbles = [i for i, rate in enumerate(change_rates) if rate < threshold]

        results["can_ids"][can_id] = {
            "frame_count": frame_count,
            "change_counts": change_counts,
            "change_rates": [round(r, 4) for r in change_rates],
            "frequent_changers": frequent_changers,
            "stable_nibbles": stable_nibbles,
            "suggested_blacklist": frequent_changers  # Nibbles to ignore in analyzer
        }

    return results


def print_report(results: dict):
    """Print a human-readable analysis report."""
    print("\n" + "=" * 70)
    print("CAN NIBBLE CHANGE ANALYSIS REPORT")
    print("=" * 70)
    print(f"Duration: {results['monitor_duration_seconds']}s")
    print(f"Total frames: {results['total_frames']}")
    print(f"Unique CAN IDs: {len(results['can_ids'])}")
    print(f"Threshold for 'frequent changer': {results['threshold_percent']}%")
    print("=" * 70)

    for can_id, data in results["can_ids"].items():
        print(f"\nCAN ID: {can_id}")
        print(f"  Frames received: {data['frame_count']}")

        # Visual representation of nibbles
        print("  Nibble map (0-15, bytes 0-7):")
        print("    Byte:    0     1     2     3     4     5     6     7")
        print("    Nibble: ", end="")
        for i in range(16):
            rate = data['change_rates'][i]
            if rate >= 0.5:
                symbol = "X"  # Very frequent changer
            elif rate >= 0.1:
                symbol = "x"  # Moderate changer
            else:
                symbol = "."  # Stable

            if i % 2 == 0:
                print(f" {symbol}", end="")
            else:
                print(f"{symbol}  ", end="")
        print()

        # Change counts
        print("    Counts: ", end="")
        for i in range(16):
            count = data['change_counts'][i]
            if i % 2 == 0:
                print(f"{count:2}", end="")
            else:
                print(f"{count:<3} ", end="")
        print()

        if data['frequent_changers']:
            nibbles_str = ", ".join(str(n) for n in data['frequent_changers'])
            bytes_affected = sorted(set(n // 2 for n in data['frequent_changers']))
            bytes_str = ", ".join(str(b) for b in bytes_affected)
            print(f"  Suggested blacklist nibbles: [{nibbles_str}] (bytes: {bytes_str})")
        else:
            print("  Suggested blacklist nibbles: [] (all stable)")

    print("\n" + "=" * 70)
    print("LEGEND: X = >50% change rate (counter/checksum)")
    print("        x = 10-50% change rate (possible counter)")
    print("        . = <10% change rate (stable/meaningful)")
    print("=" * 70)


def generate_blacklist_config(results: dict) -> dict:
    """Generate a blacklist configuration for use in CAN analyzer."""
    blacklist = {}
    for can_id, data in results["can_ids"].items():
        if data['suggested_blacklist']:
            blacklist[can_id] = data['suggested_blacklist']
    return blacklist


def main():
    parser = argparse.ArgumentParser(
        description="Monitor CAN bus and identify frequently changing nibbles (counters/checksums)",
        epilog="Output can be used to blacklist noisy nibbles in CAN analyzer tools."
    )
    parser.add_argument("port", nargs="?", help="Serial port (e.g., COM3 or /dev/ttyUSB0)")
    parser.add_argument("-b", "--baudrate", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("-d", "--duration", type=float, default=30.0,
                        help="Monitoring duration in seconds (default: 30)")
    parser.add_argument("-t", "--threshold", type=float, default=0.1,
                        help="Change rate threshold for blacklisting (default: 0.1 = 10%%)")
    parser.add_argument("-o", "--output", type=str,
                        help="Output JSON file for results")
    parser.add_argument("-c", "--config", type=str,
                        help="Output blacklist config file (simplified format)")
    parser.add_argument("-l", "--list", action="store_true",
                        help="List available serial ports")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show verbose progress output")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="Only output JSON/config, no report")

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    if not args.port:
        print("Error: No port specified. Use -l to list available ports.\n")
        parser.print_help()
        return

    # Run monitoring
    result = monitor_nibble_changes(args.port, args.baudrate, args.duration, args.verbose)

    if result is None:
        return

    can_stats, actual_duration, total_frames = result

    if not can_stats:
        print("No CAN frames received.")
        return

    # Analyze results
    analysis = analyze_results(can_stats, actual_duration, total_frames, args.threshold)

    # Print report unless quiet mode
    if not args.quiet:
        print_report(analysis)

    # Save full results to JSON if requested
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(analysis, f, indent=2)
        print(f"\nFull results saved to: {args.output}")

    # Save blacklist config if requested
    if args.config:
        blacklist = generate_blacklist_config(analysis)
        with open(args.config, 'w') as f:
            json.dump(blacklist, f, indent=2)
        print(f"Blacklist config saved to: {args.config}")

    # Print blacklist summary
    blacklist = generate_blacklist_config(analysis)
    if blacklist and not args.quiet:
        print("\nQUICK COPY - Blacklist config:")
        print(json.dumps(blacklist, indent=2))


if __name__ == "__main__":
    main()
