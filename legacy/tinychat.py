#!/usr/bin/env python3
"""Clean USB terminal chat client for TinyTalk-2 on the XIAO ESP32-S3."""
import argparse
import sys
import time

import serial
from serial.tools import list_ports

BEGIN = "@TT2:BEGIN|"
END = "@TT2:END|"


def available_ports():
    ports = list(list_ports.comports())
    return ", ".join(f"{p.device} ({p.description})" for p in ports) or "none detected"


def read_reply(board, timeout):
    """Ignore the board's local echo and startup text; parse only a reply frame."""
    deadline = time.monotonic() + timeout
    header = None
    body = []
    while time.monotonic() < deadline:
        line = board.readline().decode("utf-8", errors="replace").rstrip("\r\n")
        if not line:
            continue
        if header is None:
            if line.startswith("ERROR:"):
                raise RuntimeError(line)
            if line.startswith(BEGIN):
                header = line[len(BEGIN):]
            continue
        if line.startswith(END):
            fields = line[len(END):].split("|")
            try:
                prompt_tokens, generated, prefill_ms, decode_ms = map(int, [header, *fields[:3]])
                stats = (prompt_tokens, generated, prefill_ms, decode_ms)
            except (ValueError, TypeError):
                stats = None
            answer = "\n".join(body).strip()
            if answer.startswith("NEEDLE> "):
                answer = answer[len("NEEDLE> "):].strip()
            return answer, stats
        body.append(line)
    raise TimeoutError(f"No model reply arrived within {timeout} seconds. Check that the board finished booting and its model started.")


def clear_board_history(board, timeout=5):
    board.write(b"/new\n")
    board.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = board.readline().decode("utf-8", errors="replace")
        if "Conversation cleared." in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Chat with TinyTalk-2 running locally on your XIAO ESP32-S3.")
    parser.add_argument("port", nargs="?", default="COM9", help="XIAO serial port (default: COM9)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=int, default=180, help="maximum wait for a reply in seconds")
    args = parser.parse_args()

    try:
        board = serial.Serial(args.port, args.baud, timeout=1, write_timeout=5)
    except serial.SerialException as exc:
        print(f"Could not open {args.port}: {exc}", file=sys.stderr)
        print(f"Detected serial ports: {available_ports()}", file=sys.stderr)
        print("Close Arduino Serial Monitor, another TinyTalk terminal, or an active USB flasher, then retry.", file=sys.stderr)
        raise SystemExit(2)

    print("TinyTalk-2 · XIAO ESP32-S3 · offline USB chat")
    print(f"Connected on {args.port}. Type /new to clear memory or /quit to exit.")
    if clear_board_history(board, timeout=10):
        print("Started a fresh conversation.\n")
    else:
        print("Could not confirm a fresh conversation. Type /new before chatting if replies seem off.\n")
    try:
        while True:
            try:
                message = input("You > ").strip()
            except EOFError:
                break
            if not message:
                continue
            if message.lower() in ("/quit", "/exit"):
                break
            if message.lower() == "/new":
                if clear_board_history(board):
                    print("Conversation cleared.\n")
                else:
                    print("The board did not confirm the reset; try /new again.\n")
                continue
            if not message.isascii():
                print("This firmware accepts standard ASCII text in the USB terminal; non-ASCII characters are ignored by the board.\n")
                continue
            if len(message.encode("utf-8")) > 180:
                print("Keep messages under 180 UTF-8 bytes.\n")
                continue
            board.write((message + "\n").encode("utf-8"))
            board.flush()
            print("\nNeedle is thinking…", flush=True)
            try:
                answer, stats = read_reply(board, args.timeout)
            except (TimeoutError, RuntimeError) as exc:
                print(f"\nDevice error: {exc}\n")
                continue
            print(f"\nNEEDLE > {answer or '(empty reply)'}")
            if stats:
                p, g, pf, dec = stats
                speed = g / (dec / 1000) if dec else 0
                print(f"          {g} tokens · {speed:.1f} tok/s · prompt {p} · prefill {pf/1000:.1f}s · decode {dec/1000:.1f}s")
            print()
    except KeyboardInterrupt:
        print("\nChat closed.")
    finally:
        board.close()


if __name__ == "__main__":
    main()
