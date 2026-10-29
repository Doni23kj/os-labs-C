#!/usr/bin/env python3
"""Каждые 10 секунд дописывает текущее время в файл (для unit-файла systemd)."""
import time, sys
OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/timestamp.log"
while True:
    with open(OUT, "a") as f:
        f.write(time.strftime("%Y-%m-%d %H:%M:%S\n"))
    print("tick", flush=True)          # попадёт в journalctl
    time.sleep(10)
