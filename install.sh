#!/usr/bin/env bash
# Build and install oos-acemagic-s1 as a systemd service. Safe to run again to upgrade.
#
#   sudo ./install.sh              build, install, enable and start
#   sudo ./install.sh --no-deps    do not install build tools through the package manager
#   sudo ./install.sh --no-start   install without enabling or starting the service
#
# Packagers: set DESTDIR to stage the files without touching the running system.
set -euo pipefail

NAME=oos-acemagic-s1
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTDIR="${DESTDIR:-}"
INSTALL_DEPS=1
START=1

for arg in "$@"; do
    case "$arg" in
        --no-deps)  INSTALL_DEPS=0 ;;
        --no-start) START=0 ;;
        -h|--help)  sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

say() { printf '\n==> %s\n' "$*"; }

if [ -n "$DESTDIR" ]; then
    INSTALL_DEPS=0
    START=0
elif [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo ./install.sh" >&2
    exit 1
fi

missing_tools() {
    command -v cmake >/dev/null && command -v g++ >/dev/null && command -v make >/dev/null &&
        command -v pkg-config >/dev/null && pkg-config --exists libusb-1.0 && return 1
    return 0
}

if missing_tools; then
    if [ "$INSTALL_DEPS" -eq 0 ]; then
        echo "Missing build tools. Needed: g++, make, cmake, pkg-config, libusb-1.0 headers." >&2
        exit 1
    fi
    say "Installing build tools"
    if command -v apt-get >/dev/null; then
        apt-get update
        apt-get install -y build-essential cmake pkg-config libusb-1.0-0-dev
    elif command -v dnf >/dev/null; then
        dnf install -y gcc-c++ make cmake pkgconf-pkg-config libusb1-devel
    elif command -v pacman >/dev/null; then
        pacman -S --needed --noconfirm base-devel cmake pkgconf libusb
    elif command -v zypper >/dev/null; then
        zypper --non-interactive install gcc-c++ make cmake pkg-config libusb-1_0-devel
    else
        echo "Unknown package manager. Install g++, make, cmake, pkg-config and the libusb-1.0" >&2
        echo "headers yourself, then run: sudo ./install.sh --no-deps" >&2
        exit 1
    fi
fi

# Build as the invoking user so the build directory is not left owned by root
as_builder() {
    if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
        sudo -u "$SUDO_USER" -- "$@"
    else
        "$@"
    fi
}

say "Building"
as_builder cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
as_builder cmake --build "$SRC/build" --parallel "$(nproc)"

say "Installing"
install -Dm755 "$SRC/build/$NAME" "$DESTDIR/usr/local/bin/$NAME"
install -Dm644 "$SRC/packaging/$NAME.service" "$DESTDIR/etc/systemd/system/$NAME.service"
if [ -e "$DESTDIR/etc/$NAME/config.conf" ]; then
    echo "Keeping existing /etc/$NAME/config.conf"
else
    install -Dm644 "$SRC/packaging/config.conf" "$DESTDIR/etc/$NAME/config.conf"
fi

if [ "$START" -eq 0 ]; then
    [ -n "$DESTDIR" ] || systemctl daemon-reload
    echo "Installed. Start it with: sudo systemctl enable --now $NAME"
    exit 0
fi

# Only one program can drive the panel. s1panel is the service name used by other
# S1 panel projects; it is stopped and disabled, not removed.
for other in s1panel.service; do
    if systemctl is-active --quiet "$other" || systemctl is-enabled --quiet "$other" 2>/dev/null; then
        say "Stopping $other, which also drives the panel"
        systemctl disable --now "$other" || true
    fi
done

say "Starting the service"
systemctl daemon-reload
systemctl enable "$NAME" >/dev/null
systemctl restart "$NAME"

sleep 3
if systemctl is-active --quiet "$NAME"; then
    echo "$NAME is running. Settings: /etc/$NAME/config.conf"
else
    echo "$NAME did not stay running. Recent log:" >&2
    journalctl -u "$NAME" -n 15 --no-pager >&2 || true
    if ! grep -qs fd01 /sys/bus/usb/devices/*/idProduct; then
        echo "No panel found (USB 04d9:fd01). Is this an Acemagic S1?" >&2
    fi
    exit 1
fi
