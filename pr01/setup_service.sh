#!/usr/bin/env bash
# Запуск: sudo ./setup_service.sh
set -euo pipefail
install -d /opt/timestamp
install -m 755 timestamp.py /opt/timestamp/timestamp.py
install -m 644 timestamp.service /etc/systemd/system/timestamp.service
systemctl daemon-reload
systemctl enable --now timestamp.service
systemctl status timestamp.service --no-pager || true
sleep 25
journalctl -u timestamp -n 10 --no-pager
tail -3 /var/log/timestamp.log
