#!/bin/bash

host_src=$1
chroot_path=$2
thread_chroot_src_dir=$3
logfile=$4

# Controlla che i parametri siano stati passati
if [ -z "$host_src" ] || [ -z "$chroot_path" ] || [ -z "$thread_chroot_src_dir" ] || [ -z "$logfile" ]; then
    echo "From copySource.sh: Usage: $0 <host_src> <chroot_path> <thread_chroot_src_dir> <logfile>"
    exit 1
fi

# Reindirizza output al logfile (anche qui si tratta di un log file sull'host, non dentro il chroot - sebbene sempre un log file non condiviso ma personale del thread)
exec >> "$logfile" 2>&1
echo "From copySource.sh: Copying sources into chroot $chroot_path"

chroot_src_full_path="$chroot_path$thread_chroot_src_dir"

# Crea le directory di destinazione dentro il chroot se non esistono (in teoria dovrebbero essere già esistenti grazie alla funzione check_worker_dirs invocata dal worker)
echo "From copySource.sh: Ensuring copy point $chroot_src_full_path exists."
mkdir -p "$chroot_src_full_path"
if [ $? -ne 0 ]; then
    echo " Error: From copySource.sh: Error creating copy point $chroot_src_full_path."
    exit 1
fi

# Copio source
# Prima controllo se è già copiato per evitare errori
if [ -d "$chroot_src_full_path/.git" ]; then
    echo "From copySource.sh: $host_src already copied to $chroot_src_full_path."
else
    echo "From copySource.sh: Copying $host_src to $chroot_src_full_path"
    cp -rT "$host_src" "$chroot_src_full_path"
    if [ $? -ne 0 ]; then
        echo "Error: From copySource.sh: Error copying $host_src to $chroot_src_full_path."
        exit 1
    fi
fi

echo "From copySource.sh: Source copying for $host_src completed successfully."
exit 0
