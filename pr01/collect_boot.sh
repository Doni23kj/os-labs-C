#!/usr/bin/env bash
# Сбор данных для анализа загрузки (Ubuntu Server)
{
echo "== systemd-analyze =="; systemd-analyze
echo "== blame (top 15) =="; systemd-analyze blame | head -15
echo "== critical-chain =="; systemd-analyze critical-chain | head -20
} | tee boot_report.txt
