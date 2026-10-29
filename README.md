# Практические работы 1–8, вариант C (открытые задания) — ОС, Salymbekov University

Структура соответствует требованию сдачи: `prNN/` с `README.md`, кодом и `Makefile`/`run.sh`. Для каждой работы:
```bash
cd prNN && cat README.md      # там запуск одной командой, ожидаемые результаты и пояснения
```
Окружение: Ubuntu 22.04+, `sudo apt install -y build-essential gdb strace valgrind python3 python3-matplotlib linux-tools-generic`.

| ПР | Что внутри | Что сделать самому |
|---|---|---|
| 1 | Ubuntu Server + ssh-ключ, unit-файл systemd, сравнение init-систем | поставить ВМ, выполнить команды, вставить **свой** вывод |
| 2 | `copy.c` (read/write, fread/fwrite, sendfile, copy_file_range), `syscall_write.c`, `bench.sh` | запустить на своём ПК, построить график, `strace -c` |
| 3 | `minishell.c` (pipe, `<` `>`, `&`, `jobs`, `fg`, SIGCHLD, Ctrl+C) + `test.sh` | показать `strace -f` для `ls \| wc -l` |
| 4 | `sum_threads.c`, `spawn_cost.c`, `false_sharing.c` | замеры на многоядерной ВМ (≥4 ядер) |
| 5 | `sched.c` (SRTF, RR, MLFQ), `nice_exp.sh`, `fifo_exp.sh`, наборы set1–3 | эксперименты nice/FIFO, прочитать про EEVDF |
| 6 | `spin.c`, `rw.c`, `bounded.c` (монитор/семафоры), TSan | замеры спинлока на нескольких ядрах |
| 7 | `ipc_bench.c` (pipe, mq, shm, socket), `bench.sh` | замеры 1 ГБ, `strace -c`, `perf stat` |
| 8 | `philosophers.c` (5 вариантов), TSan/helgrind, deadlock в ядре | прогнать, проверить TSan/helgrind у себя |

## Честно о том, что проверено
* Всё собирается без предупреждений (`-Wall -Wextra`) и **запускалось** на Ubuntu 24.04, gcc 13, но на стенде с **одним ядром**. Поэтому:
  числа в таблицах README — шаблоны, которые вы заполняете своими замерами; ускорение потоков, false sharing, преимущества спинлока и livelock на 1 ядре не проявляются.
* Проверено на эталонах: SRTF и RR (q=3) на set1 совпали с «Исходными данными» (6.50/13.00 и 13.50/20.00). `./test.sh` в ПР-3 проходит. TSan чист для ПР-6; очистка IPC-ресурсов по `SIGTERM` в ПР-7 проверена.
* ПР-1 (установка ВМ/ssh/systemd) целиком выполняется на вашей машине; здесь — только файлы и инструкции.
* Перед сдачей **прочитайте код и пройдитесь по README** — на защите будут спрашивать, почему так (например: зачем блокировать `SIGCHLD`, чем TAS отличается от TTAS, почему mq режет сообщения). Пояснения для этого в README каждой работы.
