#!/bin/bash

sshlirp_source_dir=$1
sshlirp_repo_url=$2
libslirp_source_dir=$3
libslirp_repo_url=$4
logfile=$5

# Controllo che i parametri siano stati passati
if [ -z "$sshlirp_source_dir" ] || [ -z "$sshlirp_repo_url" ] || [ -z "$libslirp_source_dir" ] || [ -z "$libslirp_repo_url" ] || [ -z "$logfile" ]; then
    echo "From checkCommit.sh: Usage: $0 <sshlirp_source_dir> <sshlirp_repo_url> <libslirp_source_dir> <libslirp_repo_url> <logfile>"
    exit 1
fi

# Controllo che il path di clonaggio sia valido
if [ ! -d "$sshlirp_source_dir" ]; then
    echo "From checkCommit.sh: Error: Invalid input. Please provide a valid directory: $sshlirp_source_dir"
    exit 1
fi

# Controllo che il file di log esista
if [ ! -f "$logfile" ]; then
    echo "From checkCommit.sh: Error: Logfile does not exist: $logfile"
    exit 1
fi

# Reindirizzo gli output dei comandi e gli echo nel file di log
exec >> "$logfile" 2>&1
echo "From checkCommit.sh: Checking for updates in $sshlirp_source_dir..."

# Verifico se la directory è un repository Git
if [ ! -d "$sshlirp_source_dir/.git" ]; then
    echo "From checkCommit.sh: Error: $sshlirp_source_dir is not a valid Git repository."
    exit 1
fi

# Mi sposto nella directory del repository
cd "$sshlirp_source_dir"
if [ $? -ne 0 ]; then
    echo "From checkCommit.sh: Error: Failed to change directory to $sshlirp_source_dir."
    exit 1
fi

# Ottengo l'hash del commit corrente prima del pull
echo "From checkCommit.sh: Getting current commit hash before pull..."
before_pull_hash=$(git rev-parse HEAD)
if [ $? -ne 0 ]; then
    echo "From checkCommit.sh: Error: Failed to get current commit hash before pull."
    exit 1
fi

# Effettuo il pull
echo "From checkCommit.sh: Performing git pull..."
git_pull_output=$(git pull 2>&1)
pull_status=$?

if [ $pull_status -ne 0 ]; then
    echo "From checkCommit.sh: Error: 'git pull' failed with status $pull_status."
    echo "Output from git pull: $git_pull_output"
    exit 1 # Errore durante il pull
fi
echo "From checkCommit.sh: 'git pull' completed."
echo "Output from git pull: $git_pull_output"


# Ottengo l'hash del commit corrente dopo il pull
echo "From checkCommit.sh: Getting current commit hash after pull..."
after_pull_hash=$(git rev-parse HEAD)
if [ $? -ne 0 ]; then
    echo "From checkCommit.sh: Error: Failed to get current commit hash after pull."
    exit 1
fi

# Confronto gli hash
if [ "$before_pull_hash" != "$after_pull_hash" ]; then
    echo "From checkCommit.sh: Updates found."

    # Effettuo il pull anche di libslirp per garantire che sia aggiornato
    echo "From checkCommit.sh: Updating libslirp..."
    
    # Controllo che la directory di libslirp sia valida
    if [ ! -d "$libslirp_source_dir" ]; then
        echo "From checkCommit.sh: Error: Invalid input. Please provide a valid directory for libslirp: $libslirp_source_dir"
        exit 1
    fi

    # Mi sposto nella directory di libslirp
    cd "$libslirp_source_dir"
    if [ $? -ne 0 ]; then
        echo "From checkCommit.sh: Error: Failed to change directory to $libslirp_source_dir."
        exit 1
    fi

    # Effettuo direttamente il pull di libslirp
    git_pull_output=$(git pull 2>&1)
    pull_status=$?
    if [ $pull_status -ne 0 ]; then
        echo "From checkCommit.sh: Error: 'git pull' for libslirp failed with status $pull_status."
        echo "Output from git pull: $git_pull_output"
        exit 1 # Errore durante il pull di libslirp
    fi
    echo "From checkCommit.sh: 'git pull' for libslirp completed."
    
    exit 2 # Nuovo commit/aggiornamento scaricato
else
    echo "From checkCommit.sh: Repository is already up to date."
    exit 0 # Nessun nuovo commit
fi