#!/bin/bash

arch=$1
chroot_path=$2
logfile=$3
wrapper_script=$4
sudo_user=$5

# Controlla che i parametri siano stati passati
if [ -z "$arch" ] || [ -z "$chroot_path" ] || [ -z "$logfile" ] || [ -z "$wrapper_script" ] || [ -z "$sudo_user" ]; then
    echo "From chrootSetup.sh: Usage: $0 <architecture> <chroot_path> <logfile> <wrapper_script> <sudo_user>" >&2
    exit 1
fi

# Controllo log file
if [ ! -f "$logfile" ]; then
    echo "Error: From chrootSetup.sh: Logfile does not exist: $logfile" >&2
    exit 1
fi

exec >>"$logfile" 2>&1
echo "From chrootSetup.sh: (rootless) starting setup for $arch at $chroot_path"

# Se il rootfs sembra già pronto (esiste _enter e una home) esco
if [ -d "$chroot_path/home" ] && [ -x "$chroot_path/_enter" ]; then
    echo "From chrootSetup.sh: Rootfs already present for $arch. Skipping debootstrap."
    exit 0
fi

# Controllo wrapper
if [ ! -x "$wrapper_script" ]; then
    echo "From chrootSetup.sh: Making wrapper executable: $wrapper_script"
    chmod +x "$wrapper_script" 2>/dev/null || true
fi
if [ ! -x "$wrapper_script" ]; then
    echo "Error: From chrootSetup.sh: rootless-debootstrap-wrapper script not found or not executable at $wrapper_script" >&2
    exit 1
fi

# Scelta suite
if [ "$arch" = "arm64" ]; then
    suite="bookworm"
else
    suite="trixie"
fi

mirror="http://deb.debian.org/debian"

echo "From chrootSetup.sh: Running rootless debootstrap (suite=$suite arch=$arch mirror=$mirror)..."
"$wrapper_script" --target-dir "$chroot_path" --suite "$suite" --mirror "$mirror" --arch "$arch" --sudo-user "$sudo_user"
status=$?
if [ $status -ne 0 ]; then
    echo "Error: From chrootSetup.sh: rootless debootstrap failed (exit $status)."
    exit 1
fi

echo "From chrootSetup.sh: Rootless debootstrap completed. Verifying _enter script..."
if [ ! -x "$chroot_path/_enter" ]; then
    echo "Error: From chrootSetup.sh: _enter script missing in $chroot_path" >&2
    exit 1
fi

echo "From chrootSetup.sh: Ensuring basic directories exist..."
mkdir -p "$chroot_path/home" || true

echo "From chrootSetup.sh: Chroot (rootless) setup completed successfully for $arch at $chroot_path."
exit 0
