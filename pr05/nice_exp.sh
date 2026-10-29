#!/usr/bin/env bash
# Эксперимент: 4 CPU-bound процесса с разными nice на ОДНОМ ядре; доля CPU из /proc/PID/schedstat против весов CFS.
# Запуск: ./nice_exp.sh [секунды=20]     (nice > 0 не требует root)
set -euo pipefail
cd "$(dirname "$0")"; make -s burn
DUR=${1:-20}
NICES=(0 5 10 15)
# веса из kernel/sched/core.c (sched_prio_to_weight)
declare -A W=( [0]=1024 [5]=335 [10]=110 [15]=36 )
pids=()
for n in "${NICES[@]}"; do
  taskset -c 0 nice -n "$n" ./burn & pids+=($!)
done
sleep "$DUR"
declare -A RT
total=0; totw=0
for i in "${!pids[@]}"; do
  rt=$(awk '{print $1}' /proc/${pids[$i]}/schedstat)     # 1-е поле: время на CPU, нс
  RT[$i]=$rt; total=$((total + rt)); totw=$((totw + W[${NICES[$i]}]))
done
printf "%-6s %-8s %-14s %-12s %-12s\n" nice weight cpu_time_ms measured_% expected_%
for i in "${!pids[@]}"; do
  n=${NICES[$i]}
  awk -v n=$n -v w=${W[$n]} -v rt=${RT[$i]} -v tot=$total -v tw=$totw \
    'BEGIN{printf "%-6d %-8d %-14.0f %-12.1f %-12.1f\n", n, w, rt/1e6, 100*rt/tot, 100*w/tw}'
done
kill "${pids[@]}" 2>/dev/null || true; wait 2>/dev/null || true
