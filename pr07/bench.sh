#!/usr/bin/env bash
# TOTAL=1073741824 (1 ГБ) по умолчанию. Быстрая проверка: TOTAL=67108864 ./bench.sh
set -euo pipefail
cd "$(dirname "$0")"; make -s
TOTAL=${TOTAL:-1073741824}
echo "method,msg_size,total_bytes,seconds,MB_per_s,us_per_msg" | tee ipc_results.csv
for m in pipe mq shm sock; do
  for s in 64 4096 1048576; do
    ./ipc_bench $m $TOTAL $s | tee -a ipc_results.csv
  done
done
make -s check
