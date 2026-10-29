#!/usr/bin/env bash
# Автотест (неинтерактивный): проверяет pipe, перенаправления, фон, уборку зомби.
cd "$(dirname "$0")"; make -s || exit 1
set -u
TMP_REAL=$(cd /tmp && pwd -P)
OUT=$(printf 'echo hello | tr a-z A-Z\nls | wc -l\necho abc > /tmp/msh_t.txt\ncat < /tmp/msh_t.txt\nsleep 1 &\nsleep 2\njobs\ncd /tmp\npwd\nexit\n' | ./minishell 2>&1)
echo "$OUT"
echo "$OUT" | grep -q '^HELLO$'   || { echo "FAIL pipe"; exit 1; }
echo "$OUT" | grep -q '^abc$'     || { echo "FAIL redirect"; exit 1; }
echo "$OUT" | grep -Fqx "$TMP_REAL" || { echo "FAIL cd"; exit 1; }
rm -f /tmp/msh_t.txt
sleep 0.2; Z=$(ps -eo stat,ppid | awk -v p=$$ '$1 ~ /^Z/ && $2==p' | wc -l)
[ "$Z" -eq 0 ] && echo "OK: зомби не осталось" || { echo "FAIL: zombie"; exit 1; }
