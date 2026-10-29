# ПР-1 (вариант C): минимальная Ubuntu Server, ssh-ключ, systemd

> Эта работа — администрирование ВМ, поэтому команды нужно выполнить самому
> и вставить **свой** вывод (текстом, не скриншотами) туда, где стоит `<<ВСТАВИТЬ>>`.

## 1. Установка и ssh по ключу
1. VirtualBox → Ubuntu Server 22.04 (без GUI), сеть: Bridged или NAT + Port Forwarding (host 2222 → guest 22).
2. При установке отметить *Install OpenSSH server*.
3. На основной машине:
   ```bash
   ssh-keygen -t ed25519 -C "student@pr01"
   ssh-copy-id -p 2222 USER@127.0.0.1      # или без -p при Bridged
   ssh -p 2222 USER@127.0.0.1 'uname -a'
   ```
4. Отключить пароль: в `/etc/ssh/sshd_config` → `PasswordAuthentication no`, затем
   `sudo systemctl restart ssh`. (Сначала убедитесь, что вход по ключу работает!)

Вывод: `<<ВСТАВИТЬ>>`

## 2. Сравнение систем инициализации
| Задача | systemd (Ubuntu) | OpenRC (Alpine) | SysV init |
|---|---|---|---|
| Описание службы | unit-файл `/etc/systemd/system/x.service` (декларативный INI) | shell-скрипт `/etc/init.d/x` с `depend()` и `command=` | shell-скрипт `/etc/init.d/x` (start/stop/status) |
| Запуск / стоп | `systemctl start/stop x` | `rc-service x start/stop` | `service x start` |
| Автозапуск | `systemctl enable x` | `rc-update add x default` | `update-rc.d x defaults` (симлинки `/etc/rcN.d/S..`) |
| Статус | `systemctl status x` | `rc-service x status` | `service x status` (если скрипт реализует) |
| Журнал | `journalctl -u x` (бинарный журнал) | файлы `/var/log/*` (syslog) | файлы `/var/log/*` (syslog) |
| Зависимости | `After=`, `Requires=`, `WantedBy=`; параллельный запуск | `need`, `use`, `after` | порядок номеров S01…S99, последовательно |
| Перезапуск при падении | `Restart=on-failure` | `supervise-daemon` | нет (нужен внешний supervisor) |

## 3. Служба systemd
Файлы: `timestamp.py`, `timestamp.service`, `setup_service.sh`.
```bash
sudo ./setup_service.sh
journalctl -u timestamp -n 20 --no-pager
```
Вывод: `<<ВСТАВИТЬ>>`

Пояснения: `Type=simple` — процесс из `ExecStart` и есть служба; `Restart=on-failure` — перезапуск после аварии;
`WantedBy=multi-user.target` + `enable` создаёт симлинк, поэтому служба стартует при загрузке;
stdout скрипта попадает в журнал, поэтому `journalctl -u` его показывает.

## 4. Время загрузки
```bash
./collect_boot.sh
```
Вывод: `<<ВСТАВИТЬ>>`

Что можно отключить (выбирайте **по своему `blame`**, только то, что не нужно):
- `cloud-init*.service` — если нет облака: `sudo touch /etc/cloud/cloud-init.disabled`;
- `snapd.*` — если snap не используется;
- `apt-daily*.timer`, `motd-news.timer`;
- `ModemManager.service`, `multipathd.service` — в ВМ без модемов/SAN;
- `systemd-networkd-wait-online.service` — часто самая большая задержка (ждёт сеть).
Отключение: `sudo systemctl disable --now NAME` (или `mask`). Приложите `systemd-analyze` до и после.
