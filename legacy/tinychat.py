#!/usr/bin/env python3
"""USB terminal client for TinyTalk-2 on the XIAO ESP32-S3."""
import argparse
import sys
import threading
import serial

BEGIN = "@TT2:BEGIN|"
END = "@TT2:END|"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="serial port, e.g. COM9")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()
    try:
        board = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as exc:
        print(f"Could not open {args.port}: {exc}", file=sys.stderr)
        if "Access is denied" in str(exc) or "PermissionError(13" in str(exc):
            print("COM port is already open. Close Arduino Serial Monitor/IDE upload, another TinyTalk terminal, or disconnect the browser chat's USB connection, then run this BAT again.", file=sys.stderr)
        else:
            print("Check that the XIAO is connected and that this COM port is correct.", file=sys.stderr)
        raise SystemExit(2)

    with board:
        print(f"Connected to {args.port}. Wait for the XIAO's > prompt; Ctrl+C exits.", flush=True)

        def keyboard():
            while True:
                try:
                    line = input()
                    board.write((line + "\n").encode("utf-8"))
                except (EOFError, OSError):
                    return

        threading.Thread(target=keyboard, daemon=True).start()
        decoder = __import__("codecs").getincrementaldecoder("utf-8")("replace")
        pending = ""
        replying = False
        prompt_tokens = 0
        try:
            while True:
                chunk = board.read(512)
                if chunk:
                    pending += decoder.decode(chunk)
                if not replying:
                    at = pending.find(BEGIN)
                    if at < 0:
                        keep = 0
                        for size in range(min(len(BEGIN) - 1, len(pending)), 0, -1):
                            if BEGIN.startswith(pending[-size:]):
                                keep = size
                                break
                        safe = len(pending) - keep
                        if safe:
                            sys.stdout.write(pending[:safe])
                            sys.stdout.flush()
                            pending = pending[safe:]
                        continue
                    if at:
                        sys.stdout.write(pending[:at])
                        sys.stdout.flush()
                    pending = pending[at:]
                    nl = pending.find("\n")
                    if nl < 0:
                        continue
                    try:
                        prompt_tokens = int(pending[len(BEGIN):nl].strip())
                    except ValueError:
                        prompt_tokens = 0
                    pending = pending[nl + 1:]
                    replying = True

                at = pending.find(END)
                if at < 0:
                    continue
                answer = pending[:at]
                answer = answer.replace("\r", "")
                if answer.startswith("NEEDLE> "):
                    answer = answer[len("NEEDLE> "):]
                sys.stdout.write(answer.rstrip("\n"))
                sys.stdout.write("\n")
                rest = pending[at + len(END):]
                nl = rest.find("\n")
                if nl < 0:
                    pending = END + rest
                    continue
                fields = rest[:nl].split("|")
                try:
                    generated, prefill_ms, decode_ms = map(int, fields[:3])
                    speed = generated / (decode_ms / 1000) if decode_ms else 0
                    print(f"[prompt {prompt_tokens} tokens · generated {generated} · prefill {prefill_ms / 1000:.2f}s · decode {decode_ms / 1000:.2f}s · {speed:.2f} tok/s]")
                except (ValueError, IndexError):
                    print("[reply complete]")
                pending = rest[nl + 1:]
                replying = False
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
