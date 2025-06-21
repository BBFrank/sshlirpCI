#!/bin/bash

host_sshlirp_src=$1
host_libslirp_src=$2
chroot_path=$3
chroot_sshlirp_dir=$4
chroot_libslirp_dir=$5
logfile=$6

# Controlla che i parametri siano stati passati
if [ -z "$host_sshlirp_src" ] || [ -z "$host_libslirp_src" ] || [ -z "$chroot_path" ] || [ -z "$chroot_sshlirp_dir" ] || [ -z "$chroot_libslirp_dir" ] || [ -z "$logfile" ]; then
    echo "From copySource.sh: Usage: $0 <host_sshlirp_src> <host_libslirp_src> <chroot_path> <chroot_sshlirp_dir> <chroot_libslirp_dir> <logfile>"
    exit 1
fi

# Reindirizza output al logfile (anche qui si tratta di un log file sull'host, non dentro il chroot - sebbene sempre un log file non condiviso ma personale del thread)
exec >> "$logfile" 2>&1
echo "From copySource.sh: Copying sources into chroot $chroot_path"

chroot_sshlirp_full_path="$chroot_path$chroot_sshlirp_dir"
chroot_libslirp_full_path="$chroot_path$chroot_libslirp_dir"

# Crea le directory di destinazione dentro il chroot se non esistono (in teoria dovrebbero essere già esistenti grazie alla funzione check_worker_dirs invocata dal worker)
echo "From copySource.sh: Ensuring copy point $chroot_sshlirp_full_path exists."
sudo mkdir -p "$chroot_sshlirp_full_path"
if [ $? -ne 0 ]; then
    echo " Error: From copySource.sh: Error creating copy point $chroot_sshlirp_full_path."
    exit 1
fi

echo "From copySource.sh: Ensuring copy point $chroot_libslirp_full_path exists."
sudo mkdir -p "$chroot_libslirp_full_path"
if [ $? -ne 0 ]; then
    echo " Error: From copySource.sh: Error creating copy point $chroot_libslirp_full_path."
    exit 1
fi

# Copio sshlirp source
# Prima controllo se è già copiato per evitare errori
if [ -d "$chroot_sshlirp_full_path/.git" ]; then
    echo "From copySource.sh: $host_sshlirp_src already copied to $chroot_sshlirp_full_path."
else
    echo "From copySource.sh: Copying $host_sshlirp_src to $chroot_sshlirp_full_path"
    sudo cp -r "$host_sshlirp_src"/* "$chroot_sshlirp_full_path/"
    if [ $? -ne 0 ]; then
        echo "Error: From copySource.sh: Error copying $host_sshlirp_src to $chroot_sshlirp_full_path."
        exit 1
    fi
fi

# Copio libslirp source
if [ -d "$chroot_libslirp_full_path/.git" ]; then
    echo "From copySource.sh: $host_libslirp_src already copied to $chroot_libslirp_full_path."
else
    echo "From copySource.sh: Copying $host_libslirp_src to $chroot_libslirp_full_path"
    sudo cp -r "$host_libslirp_src"/* "$chroot_libslirp_full_path/"
    if [ $? -ne 0 ]; then
        echo "Error: From copySource.sh: Error copying $host_libslirp_src to $chroot_libslirp_full_path."
        exit 1
    fi
fi

echo "From copySource.sh: Source copying completed successfully."
exit 0
