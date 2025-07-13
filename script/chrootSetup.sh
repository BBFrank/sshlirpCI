#!/bin/bash

arch=$1
chroot_path=$2
logfile=$3
qemu_static_bin="/usr/bin/qemu-${arch}-static"

# Controlla che i parametri siano stati passati
if [ -z "$arch" ] || [ -z "$chroot_path" ] || [ -z "$logfile" ]; then
    echo "From chrootSetup.sh: Usage: $0 <architecture> <chroot_path> <logfile>"
    exit 1
fi

# Controllo che qemu static binary esista
if [ ! -f "$qemu_static_bin" ]; then
    echo "Warning: From chrootSetup.sh: symlink $qemu_static_bin not found on host. Getting original binary..."
    qemu_arch=$arch
    case "$arch" in
        ("amd64") qemu_arch="x86_64" ;;
        ("arm64") qemu_arch="aarch64" ;;
        ("armhf") qemu_arch="arm" ;;
        ("armel") qemu_arch="arm" ;;
        ("ppc64el") qemu_arch="ppc64le" ;;
    esac
    
    qemu_static_bin="/usr/bin/qemu-${qemu_arch}-static"

    if [ ! -f "$qemu_static_bin" ]; then
        echo "Error: From chrootSetup.sh: $qemu_static_bin binary not found on host. Please install qemu-user-static package."
        exit 1
    fi
fi

# Reindirizza output al logfile (nota: questo è ancora il log file sull'host, non dentro il chroot)
exec >> "$logfile" 2>&1
echo "From chrootSetup.sh: Starting chroot setup for $arch at $chroot_path"

# Controlla se il chroot esiste già (semplice controllo sulla directory)
if [ -d "$chroot_path/home" ]; then
    echo "From chrootSetup.sh: Chroot directory $chroot_path already seems to be set up (home dir exists). Skipping debootstrap."
    exit 0
fi

echo "From chrootSetup.sh: Running debootstrap first stage for $arch..."
# Se siamo su arm64 devo usare bookworm come distro, non trixie -> modificato da specifiche => utilizzare trixie per tutte le arches, ma non per arm64 in quanto trixie - unstable - blocca l'unpacking del base system su arm64
if [ "$arch" = "arm64" ]; then
    echo "From chrootSetup.sh: Using bookworm as the distribution for architecture $arch."
    sudo debootstrap --arch="$arch" --foreign bookworm "$chroot_path" http://deb.debian.org/debian
else
    echo "From chrootSetup.sh: Using trixie as the distribution for architecture $arch."
    sudo debootstrap --arch="$arch" --foreign trixie "$chroot_path" http://deb.debian.org/debian
fi

if [ $? -ne 0 ]; then
    echo "Error: From chrootSetup.sh: debootstrap first stage failed for $arch."
    exit 1
fi

echo "From chrootSetup.sh: Copying $qemu_static_bin to $chroot_path/usr/bin/..."
sudo cp "$qemu_static_bin" "$chroot_path/usr/bin/"
if [ $? -ne 0 ]; then
    echo "Error: From chrootSetup.sh: Failed to copy qemu static binary."
    exit 1
fi

echo "From chrootSetup.sh: Running debootstrap second stage for $arch..."
sudo chroot "$chroot_path" /debootstrap/debootstrap --second-stage
if [ $? -ne 0 ]; then
    echo "Error: From chrootSetup.sh: debootstrap second stage failed for $arch."
    exit 1
fi

echo "From chrootSetup.sh: Chroot setup completed successfully for $arch at $chroot_path."
exit 0
