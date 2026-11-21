"""Upload a script over USB serial; execution is always started on the detector."""
from pathlib import Path
import argparse
import re
import time


def request(port, command):
    port.write((command + "\n").encode("ascii"))
    port.flush()
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if line.startswith("BADUSB_ERROR"):
            raise RuntimeError(line)
        if line.startswith("BADUSB_OK"):
            return line
    raise RuntimeError("Detector did not acknowledge the upload command")


def upload(port, name, data):
    request(port, f"badusb upload {name} {len(data)}")
    for offset in range(0, len(data), 256):
        chunk = data[offset:offset + 256]
        reply = request(port, f"badusb chunk {offset} {chunk.hex()}")
        if reply != f"BADUSB_OK offset={offset + len(chunk)}":
            raise RuntimeError(f"Unexpected upload acknowledgement: {reply}")
    request(port, "badusb save")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Native USB serial port, e.g. /dev/cu.usbmodem... or COM5")
    parser.add_argument("script", type=Path, help="ASCII DuckyScript subset file")
    parser.add_argument("--name", help="Saved filename; defaults to the source filename")
    args = parser.parse_args()
    data = args.script.read_bytes()
    name = args.name or args.script.name
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,28}\.txt", name) or name == "demo.txt":
        parser.error("Use a 1..28 character name (letters, numbers, _ or -) plus .txt; demo.txt is reserved")
    if not 1 <= len(data) <= 8192:
        parser.error("Scripts must contain 1..8192 bytes")
    try:
        data.decode("ascii")
    except UnicodeDecodeError:
        parser.error("Scripts must use ASCII; the keyboard uses the US layout")
    try:
        import serial
    except ImportError:
        parser.error("Install pyserial first: python3 -m pip install pyserial")
    # TinyUSB CDC needs DTR asserted to send replies. Assert both lines together
    # without the multi-step reset sequence; close Studio before uploading.
    with serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=5) as port:
        port.dtr = True
        port.rts = True
        port.port = args.port
        port.open()
        try:
            upload(port, name, data)
        except (RuntimeError, serial.SerialException) as error:
            raise SystemExit(str(error)) from error
    print(f"Saved {name}. Select it in BadUSB on the detector, then press Run.")


if __name__ == "__main__":
    main()
