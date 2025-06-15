#!/bin/bash

chroot_path=$1
chroot_sshlirp_dir=$2
chroot_libslirp_dir=$3
logfile=$4

# Controllo che i parametri siano stati passati
if [ -z "$chroot_path" ] || [ -z "$chroot_sshlirp_dir" ] || [ -z "$chroot_libslirp_dir" ] || [ -z "$logfile" ]; then
    echo "From removeSourceCopy.sh: Usage: $0 <chroot_path> <chroot_sshlirp_dir> <chroot_libslirp_dir> <logfile>"
    exit 1
fi

# Reindirizza output al logfile (che anche in questo caso sarà sempre il log file personale del thread ma nell'host)
exec >> "$logfile" 2>&1
echo "From removeSourceCopy.sh: Removing sources copy from chroot $chroot_path"

chroot_sshlirp_full_path="$chroot_path$chroot_sshlirp_dir"
chroot_libslirp_full_path="$chroot_path$chroot_libslirp_dir"

# Rimuovo sshlirp source senza eliminare la directory chroot_sshlirp_dir
if [ -d "$chroot_sshlirp_full_path/.git" ]; then
    echo "From removeSourceCopy.sh: Removing $chroot_sshlirp_full_path contents."
    sudo rm -rf "$chroot_sshlirp_full_path/"*
    if [ $? -ne 0 ]; then
        echo "From removeSourceCopy.sh: Error removing contents of $chroot_sshlirp_full_path."
        exit 1
    fi
else
    echo "From removeSourceCopy.sh: $chroot_sshlirp_full_path does not exist or is not a valid directory."
fi

# Rimuovo libslirp source senza eliminare la directory chroot_libslirp_dir
if [ -d "$chroot_libslirp_full_path/.git" ]; then
    echo "From removeSourceCopy.sh: Removing $chroot_libslirp_full_path contents."
    sudo rm -rf "$chroot_libslirp_full_path/"*
    if [ $? -ne 0 ]; then
        echo "From removeSourceCopy.sh: Error removing contents of $chroot_libslirp_full_path."
        exit 1
    fi
else
    echo "From removeSourceCopy.sh: $chroot_libslirp_full_path does not exist or is not a valid directory."
fi

echo "From removeSourceCopy.sh: Sources copy removed successfully from chroot $chroot_path."
exit 0
