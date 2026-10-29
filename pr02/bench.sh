#!/usr/bin/env bash
# Замер: размер буфера -> время. SIZE_MB=100 по умолчанию, REPEAT=3.
# Быстрая проверка: SIZE_MB=10 REPEAT=1 ./bench.sh
set -euo pipefail
cd "$(dirname "$0")"; make -s
SIZE_MB=${SIZE_MB:-100}; REPEAT=${REPEAT:-3}
[ -f big.tmp ] || dd if=/dev/urandom of=big.tmp bs=1M count="$SIZE_MB" status=none
echo "mode,bufsize,seconds" > results.csv
for mode in rw stdio; do
  for bs in 1 64 4096 65536; do
    for i in $(seq "$REPEAT"); do
      sync; ./copy "$mode" big.tmp out.tmp "$bs" >> results.csv
    done
  done
done
for mode in sendfile cfr; do
  for i in $(seq "$REPEAT"); do sync; ./copy "$mode" big.tmp out.tmp >> results.csv; done
done
cmp big.tmp out.tmp && echo "копия совпадает"
python3 - <<'PY'
import csv, statistics as st, collections
d = collections.defaultdict(list)
for r in csv.DictReader(open("results.csv")): d[(r["mode"], int(r["bufsize"]))].append(float(r["seconds"]))
print(f"{'mode':10}{'buf':>8}{'median s':>12}")
for (m, b), v in sorted(d.items()): print(f"{m:10}{b:>8}{st.median(v):>12.4f}")
try:
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    for m in ("rw", "stdio"):
        pts = sorted((b, st.median(v)) for (mm, b), v in d.items() if mm == m)
        plt.plot([p[0] for p in pts], [p[1] for p in pts], "o-", label=m)
    plt.xscale("log"); plt.yscale("log"); plt.xlabel("размер буфера, байт"); plt.ylabel("время, с")
    plt.legend(); plt.grid(True, which="both", alpha=.3); plt.savefig("buffer_time.png", dpi=120)
    print("график: buffer_time.png")
except ImportError:
    print("нет matplotlib: sudo apt install python3-matplotlib (или постройте график из results.csv)")
PY
rm -f out.tmp
