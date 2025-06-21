#!/bin/bash

what2clone=$1
where2clone=$2
logfile=$3
versioning_file=$4

sshlirp_git_clone=1

# Controlla che i parametri siano stati passati
if [ -z "$what2clone" ] || [ -z "$where2clone" ] || [ -z "$logfile" ]; then
    echo "From gitClone.sh: Usage: $0 <repository_url> <destination_directory> <logfile>"
    exit 1
fi

# Controlla che il file di versioning sia stato passato. Se no sto clonando libslirp, altrimenti sto clonando sshlirp e avrò bisogno di modificare il file di versioning
if [ -z "$versioning_file" ]; then
    sshlirp_git_clone=0
fi

# Controlla che il file di log esista
if [ ! -f $logfile ]; then
    echo "Error: From gitClone.sh: Logfile does not exist."
    exit 1
fi

# Reindirizza gli output dei comandi e gli echo nel file di log
exec >> $logfile 2>&1

# Controlla che il path di clonaggio sia valido
if [ ! -d $where2clone ]; then
    echo "Error: From gitClone.sh: Invalid input. Please provide a valid directory."
    exit 1
fi

if [ $sshlirp_git_clone -eq 1 ]; then
    # Controllo che il file di versioning esista
    if [ ! -f $versioning_file ]; then
        echo "Error: From gitClone.sh (for sshlirp cloning): Versioning file does not exist: $versioning_file"
        exit 1
    fi
fi

# Controlla se nella directory c'è già un repository Git
if [ -d "$where2clone/.git" ]; then
    echo "From gitClone.sh: Info: $where2clone already contains a Git repository."
    exit 0 # non ho nulla da clonare
fi

# Clona il repository
echo "From gitClone.sh: Cloning form $what2clone into $where2clone..."
git clone $what2clone $where2clone
if [ $? -ne 0 ]; then
    echo "Error: From gitClone.sh: Failed to clone repository."
    exit 1
fi

if [ $sshlirp_git_clone -eq 1 ]; then
    # Verifica se c'è un tag e nel caso scrivilo nel file di versioning
    cd $where2clone
    if [ $? -ne 0 ]; then
        echo "Error: From gitClone.sh (for sshlirp cloning): Failed to change directory to $where2clone."
        exit 1
    fi
    current_tag=$(git describe --tags --abbrev=0)

    if [ -n "$current_tag" ]; then
        echo "From gitClone.sh (for sshlirp cloning): Found current tag: $current_tag"
        echo "From gitClone.sh (for sshlirp cloning): Writing current tag to versioning file: $versioning_file"
        echo "$current_tag" >> $versioning_file
    else
        echo "From gitClone.sh (for sshlirp cloning): No tags found."
    fi
fi

echo "From gitClone.sh: Cloning completed successfully."
exit 2 # ho effettuato il clone