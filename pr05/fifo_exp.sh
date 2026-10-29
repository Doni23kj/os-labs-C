#!/usr/bin/env bash
# Эксперимент: один процесс в SCHED_FIFO (chrt -f 10) против трёх обычных (SCHED_OTHER) на одном ядре.
# Нужен root:  sudo ./fifo_exp.sh [секунды=10]
# ВНИМАНИЕ: делайте в ВМ. Ядро по умолчанию ограничивает RT-время 95% периода (sched_rt_runtime_us=950000),
# поэтому система не зависнет, но при отключении этого лимита (-1) зависнуть может.
set -euo pipefail
[ "$(id -u)" = 0 ] || { echo "нужен root: sudo $0"; exit 1; }
cd "$(dirname "$0")"; make -s burn
DUR=${1:-10}
echo "sched_rt_runtime_us=$(cat /proc/sys/kernel/sched_rt_runtime_us) sched_rt_period_us=$(cat /proc/sys/kernel/sched_rt_period_us)"
taskset -c 0 chrt -f 10 ./burn & rt=$!
others=()
for i in 1 2 3; do taskset -c 0 ./burn & others+=($!); done
sleep "$DUR"
all=($rt "${others[@]}")
declare -A T POL; tot=0
for p in "${all[@]}"; do                       # снимаем все значения один раз, в один момент
  T[$p]=$(awk '{print $1}' /proc/$p/schedstat); POL[$p]=$(chrt -p $p | head -1 | sed 's/.*policy: //')
  tot=$((tot + T[$p]))
done
echo "процесс        политика      cpu_ms   доля_%"
for p in "${all[@]}"; do
  awk -v p=$p -v pol="${POL[$p]}" -v t=${T[$p]} -v tot=$tot 'BEGIN{printf "%-14s %-13s %-8.0f %-6.1f\n", p, pol, t/1e6, 100*t/tot}'
done
kill $rt "${others[@]}" 2>/dev/null || true; wait 2>/dev/null || true
