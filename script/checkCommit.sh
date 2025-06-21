#!/bin/bash

sshlirp_source_dir=$1
sshlirp_repo_url=$2
libslirp_source_dir=$3
libslirp_repo_url=$4
logfile=$5
versioning_file=$6

# Controllo che i parametri siano stati passati
if [ -z "$sshlirp_source_dir" ] || [ -z "$sshlirp_repo_url" ] || [ -z "$libslirp_source_dir" ] || [ -z "$libslirp_repo_url" ] || [ -z "$logfile" ] || [ -z "$versioning_file" ]; then
    echo "From checkCommit.sh: Usage: $0 <sshlirp_source_dir> <sshlirp_repo_url> <libslirp_source_dir> <libslirp_repo_url> <logfile> <versioning_file>"
    exit 1
fi

# Controllo che il path di clonaggio sia valido
if [ ! -d $sshlirp_source_dir ]; then
    echo "Error: From checkCommit.sh: Invalid input. Please provide a valid directory: $sshlirp_source_dir"
    exit 1
fi

# Controllo che il file di log esista
if [ ! -f $logfile ]; then
    echo "Error: From checkCommit.sh: Logfile does not exist: $logfile"
    exit 1
fi

# Reindirizzo gli output dei comandi e gli echo nel file di log
exec >> $logfile 2>&1
echo "From checkCommit.sh: Checking for updates in $sshlirp_source_dir..."

# Controllo che il file di versioning esista
if [ ! -f $versioning_file ]; then
    echo "Error: From checkCommit.sh: Versioning file does not exist: $versioning_file"
    exit 1
fi

# Verifico se la directory è un repository Git
if [ ! -d "$sshlirp_source_dir/.git" ]; then
    echo "Error: From checkCommit.sh: $sshlirp_source_dir is not a valid Git repository."
    exit 1
fi

# Mi sposto nella directory del repository
cd $sshlirp_source_dir
if [ $? -ne 0 ]; then
    echo "Error: From checkCommit.sh: Failed to change directory to $sshlirp_source_dir."
    exit 1
fi

# Ottengo l'hash del commit corrente prima del pull
echo "From checkCommit.sh: Getting current commit hash before pull..."
before_pull_hash=$(git rev-parse HEAD)
if [ $? -ne 0 ]; then
    echo "Error: From checkCommit.sh: Failed to get current commit hash before pull."
    exit 1
fi

# Ottengo l'ultimo tag che puntava a un commit che ho già scaricato (ultimo tag prima del pull)
echo "From checkCommit.sh: Getting current tag..."
current_tag=$(git describe --tags --abbrev=0)
if [ $? -ne 0 ]; then
    echo "From checkCommit.sh: No tags found in the local repository."
else
    echo "From checkCommit.sh: Current tag is $current_tag."
fi

# Effettuo il pull
echo "From checkCommit.sh: Performing git pull..."
git_pull_output=$(git pull 2>&1)
pull_status=$?

if [ $pull_status -ne 0 ]; then
    echo "Error: From checkCommit.sh: 'git pull' failed with status $pull_status."
    echo "Output from git pull: $git_pull_output"
    exit 1
fi
echo "From checkCommit.sh: 'git pull' completed."
echo "Output from git pull: $git_pull_output"


# Ottengo l'hash del commit corrente dopo il pull
echo "From checkCommit.sh: Getting current commit hash after pull..."
after_pull_hash=$(git rev-parse HEAD)
if [ $? -ne 0 ]; then
    echo "Error: From checkCommit.sh: Failed to get current commit hash after pull."
    exit 1
fi

# Ottengo l'ultimo tag dopo il pull
echo "From checkCommit.sh: Getting current tag after pull..."
current_tag_after_pull=$(git describe --tags --abbrev=0)
if [ $? -ne 0 ]; then
    echo "From checkCommit.sh: No new tag found after pull."
else
    echo "From checkCommit.sh: Current tag after pull is $current_tag_after_pull."
fi

# Confronto gli hash (per capire se ci sono stati aggiornamenti)
if [ "$before_pull_hash" != "$after_pull_hash" ]; then
    echo "From checkCommit.sh: Updates found."

    # Se ho anche un nuovo tag (tag diverso dal current e non vuoto), lo inserisco nel file di versioning
    if [ "$current_tag_after_pull" != "$current_tag" ] && [ -n "$current_tag_after_pull" ]; then
        echo "From checkCommit.sh: Updating versioning file."
        echo "$current_tag_after_pull" >> $versioning_file
    fi

    # Effettuo il pull anche di libslirp per garantire che sia aggiornato
    echo "From checkCommit.sh: Updating libslirp..."
    
    # Controllo che la directory di libslirp sia valida
    if [ ! -d $libslirp_source_dir ]; then
        echo "Error: From checkCommit.sh: Invalid input. Please provide a valid directory for libslirp: $libslirp_source_dir"
        exit 1
    fi

    # Mi sposto nella directory di libslirp
    cd $libslirp_source_dir
    if [ $? -ne 0 ]; then
        echo "Error: From checkCommit.sh: Failed to change directory to $libslirp_source_dir."
        exit 1
    fi

    # Effettuo direttamente il pull di libslirp
    git_pull_output=$(git pull 2>&1)
    pull_status=$?
    if [ $pull_status -ne 0 ]; then
        echo "Error: From checkCommit.sh: 'git pull' for libslirp failed with status $pull_status."
        echo "Output from git pull: $git_pull_output"
        exit 1 # Errore durante il pull di libslirp
    fi
    echo "From checkCommit.sh: 'git pull' for libslirp completed."
    
    exit 2 # Nuovo commit/aggiornamento scaricato
else
    echo "From checkCommit.sh: Repository is already up to date."
    exit 0 # Nessun nuovo commit
fi