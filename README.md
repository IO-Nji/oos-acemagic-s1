# oos-acemagic-s1

A system dashboard for the front display of the **Acemagic S1** mini PC on Linux.
It runs as a small systemd service and rotates through pages of system information
on the built-in 320x170 panel, styled after the oOS embedded UI (Orbitron type, navy
and cyan).

![The six default dashboard pages](docs/screenshots/overview.png)

*Screenshots are rendered by the bundled preview tool from sample data.*

- **System**: CPU load, frequency, load average and temperature; memory with a
  used / cache / buffers bar; disk use; CPU package power; uptime
- **Network**: primary address, per-interface speeds and Wi-Fi signal, totals since
  boot, and download / upload history graphs
- **Sensors**: CPU and SSD (or GPU) temperature gauges with history; power by
  domain; every other temperature sensor
- **Docker**: running / total counts and the containers that matter most:
  unhealthy first, then the busiest
- **Services**: state of the systemd units you choose
- **Identity**: hostname, clock, OS, kernel, uptime, login sessions and alerts
- **MQTT** (optional, off by default): for machines that run or use an MQTT broker —
  connected clients, message rates, broker uptime, and a running log of messages
  by topic

The footer of every page shows the machine's health: `OK`, or the most serious
alert and how many more there are.

| Healthy | With alerts | MQTT page |
| --- | --- | --- |
| ![Identity page](docs/screenshots/identity.png) | ![Identity page showing alerts](docs/screenshots/alerts.png) | ![MQTT page](docs/screenshots/mqtt.png) |

**Other displays.** The same dashboard runs on a Raspberry Pi with a 3.5" SPI touch
display: [oos-rpi-lcd35](https://github.com/IO-Nji/oos-rpi-lcd35). It uses this
repository's code and adds touch navigation.

## Requirements

- An Acemagic S1 (or a rebrand with the same front panel). Check with:

  ```
  lsusb -d 04d9:fd01
  ```

  If that prints a "Holtek Semiconductor" line, the panel is there.
- Linux with systemd. Developed and tested on Ubuntu 24.04; the installer also
  knows `dnf`, `pacman` and `zypper`, but those paths have not been tested.
- Optional: Docker for the Docker page; an Intel CPU for the power readings.

## Install

```
git clone https://github.com/IO-Nji/oos-acemagic-s1.git
cd oos-acemagic-s1
sudo ./install.sh
```

The installer:

1. installs the build tools if they are missing (a C++17 compiler, CMake,
   pkg-config and the libusb-1.0 headers),
2. builds the program and runs its tests,
3. installs `/usr/local/bin/oos-acemagic-s1`, the systemd unit, and
   `/etc/oos-acemagic-s1/config.conf`,
4. stops and disables a service called `s1panel` if one is running, because only one
   program can drive the panel (nothing is deleted),
5. enables and starts the service.

The display should come up within a few seconds. Check on it with:

```
systemctl status oos-acemagic-s1
journalctl -u oos-acemagic-s1 -f
```

To upgrade, `git pull` and run `sudo ./install.sh` again; your settings are kept.

Options: `--no-deps` skips the package manager, `--no-start` installs without
starting the service.

## Configure

Edit `/etc/oos-acemagic-s1/config.conf`, then `sudo systemctl restart oos-acemagic-s1`.

```ini
# Seconds each page stays on screen (2 to 3600)
page_interval = 5

# Pages to show, in this order. Leave one out to hide it.
# Available: system, network, sensors, docker, services, identity, mqtt
pages = system, network, sensors, docker, services, identity

# systemd units listed on the Services page. Units that do not exist are skipped.
services = ssh, docker, nginx, apache2, mysql, mariadb, postgresql, redis, mosquitto, oos-acemagic-s1

# Filesystem shown in the DISK card and watched for the disk alert
disk = /

# MQTT page: add "mqtt" to pages to show it
mqtt_host = 127.0.0.1
mqtt_port = 1883
#mqtt_user =
#mqtt_password =
mqtt_topics = #        # topic filters to list, comma separated
```

Every line is optional. A mistake in the file is reported in the journal and the
affected setting keeps its default; the service still starts.

### MQTT page

Add `mqtt` to `pages` to show it. The dashboard connects to the broker as a
read-only client (it never publishes), subscribes to `mqtt_topics` and to the
broker's `$SYS` statistics, and reconnects on its own.

- The top card shows connected clients and messages in and out per minute, or
  `BROKER OFFLINE` with the broker's address.
- The log lists each message as it arrives, topic then payload, newest at the
  bottom. Long topics are shortened in the middle so both ends stay readable.
  Retained messages are drawn in a dimmer colour than live ones.
- `mqtt_topics` takes MQTT topic filters, comma separated, for example
  `home/#, sensors/+/state`. The default `#` lists everything.

The client count includes the dashboard itself. If the broker needs a login, note
that the settings file is readable by every user unless you restrict it
(`sudo chmod 600 /etc/oos-acemagic-s1/config.conf`).

## Alerts

| Footer text | Raised when | Level |
| --- | --- | --- |
| `MQTT OFFLINE` | the MQTT page is enabled and its broker cannot be reached | critical |
| `n SVC FAILED` | a watched systemd unit is in the failed state | critical |
| `DISK nn%` | the watched filesystem is 90 % full (97 %: critical) | warning |
| `CPU nnC` | the CPU package reaches 85 C (95 C: critical) | warning |
| `LOW VOLTAGE` | a Raspberry Pi reports that its supply voltage is too low | warning |
| `n CTR UNHEALTHY` | a container is unhealthy, restarting or dead | warning |
| `MEM nn%` | memory in use reaches 92 % | warning |

Colours mean the same thing everywhere: green is fine, amber needs a look, red is a
problem. Gauges and percentages turn amber at 60 % and red at 85 %; temperatures at
60 C and 85 C.

## Uninstall

```
sudo ./uninstall.sh            # keeps /etc/oos-acemagic-s1
sudo ./uninstall.sh --purge    # removes the settings too
```

## Troubleshooting

**The service keeps restarting with "Cannot open the S1 panel".**
Run `lsusb -d 04d9:fd01`. No output means the machine has no S1 panel. If the
journal says the interface is busy, another program is driving the display; stop it.

**The panel shows a "disconnection" notice.**
That is the panel's own screen when nothing is talking to it. Start the service.

**POWER shows N/A.**
Power comes from Intel RAPL counters, which only root can read. The service runs as
root; the preview tool run as a normal user will show N/A. AMD CPUs are not supported.

**FANS shows N/A.**
The S1 does not expose its fan to the operating system.

**Docker page says "Docker not available".**
The `docker` command is missing or the daemon is not running.

**MQTT page says "BROKER OFFLINE".**
Nothing accepted the connection at `mqtt_host`:`mqtt_port`, or the broker refused the
login. Try `mosquitto_sub -h HOST -p PORT -t '$SYS/broker/version' -C 1` from the
same machine. If client counts and rates stay empty, the broker does not publish
`$SYS` statistics.

**A page change wipes down the screen.**
The panel's USB link moves 64 bytes per millisecond, so a full repaint takes about two
seconds. Small changes (the clock, a value) take a fraction of that. See
[docs/PROTOCOL.md](docs/PROTOCOL.md).

## How it works

```
src/core   drawing, Orbitron glyphs, theme, settings, changed-region tracking
src/app    the dashboard assembled from the parts below, without any hardware
src/hw     USB device and the panel transport
src/data   providers (system, network, sensors, docker, services, identity), the
           MQTT monitor, background collection, health evaluation
src/ui     page frame and the pages
tools      offline preview renderer, font baking script
tests      unit tests
```

- Data is collected on background threads, each source at its own pace (1 s for
  CPU, memory and network; 2 s for sensors; 10 s for Docker and systemd), so
  drawing never waits on a slow command.
- The display is updated by sending only the rectangles that changed. The panel's
  full-screen redraw command is avoided because the firmware stops responding for
  several seconds after each one; the details are in [docs/PROTOCOL.md](docs/PROTOCOL.md).
- The service uses about 1.5 % of one core and under 15 MB of memory, including the
  `docker` and `systemctl` commands it runs.
- The MQTT page uses a small built-in MQTT 3.1.1 client, so there is nothing extra
  to install.
- Pages are laid out once, in units of the S1's 320x170 panel. Everything except
  `src/hw` and `src/main.cpp` is device-independent; see "Other devices" below.
- It runs as root (needed for the power counters and the Docker socket) with
  `NoNewPrivileges`, a read-only system and home, and a private `/tmp`.

## Development

Build without installing:

```
cmake -S . -B build && cmake --build build -j
```

The build runs the unit tests and a layout check that fails if any text overlaps,
leaves the frame, or a theme colour has less than 4.5:1 contrast. On a machine
without libusb, add `-DOOS_BUILD_SERVICE=OFF` to build only the preview tool and tests.

Render the pages to PNG without a panel:

```
build/oos-s1-preview --live --out DIR                      # this machine's data
build/oos-s1-preview --mock typical --scale 2 --out DIR    # sample data
build/oos-s1-preview --mock stress --out DIR               # worst-case widths
build/oos-acemagic-s1 --dry-run                            # full loop, no USB
```

Changing the look:

- **Colours** live in `src/core/theme.hpp` as named roles; pages never use raw colours.
- **Fonts**: sizes and weights are set in `tools/bake_fonts.py`. Download the
  [Orbitron](https://fonts.google.com/specimen/Orbitron) static TTFs, then run
  `python3 tools/bake_fonts.py path/to/Orbitron/static` (needs Pillow) to regenerate
  `src/core/font_orbitron.cpp`.

## Other devices

Another display can reuse everything except `src/hw` and `src/main.cpp` by adding
this repository as a subdirectory (or git submodule) and linking `oos_core`.
[oos-rpi-lcd35](https://github.com/IO-Nji/oos-rpi-lcd35) is a complete example. A
device chooses:

- its **scale** (`UiScale`): fonts are baked for 100 % and 150 %; add a scale in
  `tools/bake_fonts.py`,
- its **layout** (`Chrome` in `src/core/layout.hpp`): framed or not, margins, the gap
  between cards, and whether card content is centred,
- its **palette**: build with `OOS_THEME_HIGH_CONTRAST` for panels with little
  contrast,
- and supplies its own output, input and `main()`. `tools/preview_app.hpp` gives it
  the same preview tool and layout checks.

The defaults are the S1, whose output these options do not change.

## Credits and licence

- Panel protocol: [tjaworski/AceMagic-S1-LED-TFT-Linux](https://github.com/tjaworski/AceMagic-S1-LED-TFT-Linux).
- Font: [Orbitron](https://fonts.google.com/specimen/Orbitron) by The Orbitron Project Authors,
  SIL Open Font License 1.1 (`third_party/Orbitron-OFL.txt`).

Licensed under the [GNU General Public License v3.0](LICENSE).

This project is not affiliated with Acemagic.
