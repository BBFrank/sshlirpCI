#!/bin/bash

what2clone=$1
where2clone=$2
logfile=$3

# Controlla che i parametri siano stati passati
if [ -z "$what2clone" ] || [ -z "$where2clone" ] || [ -z "$logfile" ]; then
    echo "From gitClone.sh: Usage: $0 <repository_url> <destination_directory> <logfile>"
    exit 1
fi

# Controlla che il file di log esista
if [ ! -f $logfile ]; then
    echo "From gitClone.sh: Error: Logfile does not exist."
    exit 1
fi

# Reinderizza gli output dei comandi e gli echo nel file di log
exec >> "$logfile" 2>&1

# Controlla che il path di clonaggio sia valido
if [ ! -d $where2clone ]; then
    echo "From gitClone.sh: Error: Invalid input. Please provide a valid directory."
    exit 1
fi

# Controlla se nella directory c'è già un repository Git
if [ -d "$where2clone/.git" ]; then
    echo "From gitClone.sh: Info: $where2clone already contains a Git repository."
    exit 0
fi

echo "From gitClone.sh: Cloning form $what2clone into $where2clone..."
git clone $what2clone $where2clone
if [ $? -ne 0 ]; then
    echo "From gitClone.sh: Error: Failed to clone repository."
    exit 1
fi

echo "From gitClone.sh: Cloning completed successfully."
exit 0