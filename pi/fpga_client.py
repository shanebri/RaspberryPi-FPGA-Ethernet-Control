#!/usr/bin/env python3

import argparse
import socket

DEFAULT_FPGA_IP = "192.168.1.10"
DEFAULT_PORT = 7


def send_value(ip: str, port: int, value: int) -> None:
    if not 0 <= value <= 15:
        raise ValueError("Value must be between 0 and 15.")

    message = f"{value}\n".encode()

    print(f"Connecting to FPGA at {ip}:{port}")
    print(f"Sending {value} -> {value:04b}")

    with socket.create_connection((ip, port), timeout=5) as sock:
        sock.sendall(message)
        response = sock.recv(1024)

    print(f"FPGA response: {response.decode().strip()}")
    print(f"LED pattern:   {value:04b}")


def main():
    parser = argparse.ArgumentParser(
        description="Control the four LEDs on an Arty A7 FPGA over Ethernet."
    )

    parser.add_argument(
        "value",
        type=int,
        help="Decimal value to display on the FPGA LEDs (0-15)",
    )

    parser.add_argument(
        "--ip",
        default=DEFAULT_FPGA_IP,
        help=f"FPGA IPv4 address (default: {DEFAULT_FPGA_IP})",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"FPGA TCP port (default: {DEFAULT_PORT})",
    )

    args = parser.parse_args()

    try:
        send_value(args.ip, args.port, args.value)
    except ValueError as exc:
        parser.error(str(exc))
    except (ConnectionError, socket.timeout, OSError) as exc:
        print(f"Communication failed: {exc}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()