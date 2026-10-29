#!/usr/bin/env bash
# Сводная таблица SRTF / RR(q=3) / MLFQ(q=4: 4,8,16; boost=50) на всех наборах
cd "$(dirname "$0")"; make -s
printf "%-6s %-8s %10s %10s %10s %8s\n" set algo wait turnaround response ctx
for s in set1 set2 set3; do
  for a in "srtf" "rr --quantum 3" "mlfq --quantum 4 --boost 50"; do
    ./sched --algo $a $s.txt | tail -1 | awk -v s=$s '{split($1,a,"=");split($2,w,"=");split($3,t,"=");split($4,r,"=");split($6,c,"=");
      printf "%-6s %-8s %10s %10s %10s %8s\n", s, a[2], w[2], t[2], r[2], c[2]}'
  done
done
