#!/bin/bash

vdens_c_path=$1
log_file=$2

# Controllo se i parametri sono stati passati
if [ -z "$vdens_c_path" ] || [ -z "$log_file" ]; then
    echo "Usage: $0 <path_to_vdens_c> <log_file>"
    exit 1
fi

# Controlla che il file di log esista
if [ ! -f $log_file ]; then
    echo "Error: From gitClone.sh: Logfile does not exist."
    exit 1
fi

# Reindirizza gli output dei comandi e gli echo nel file di log
exec >> $log_file 2>&1

# Controlla che il file sorgente di vdens esista
if [ ! -f "$vdens_c_path" ]; then
    echo "Error: From modifyVdens.sh: vdens.c file does not exist at $vdens_c_path"
    exit 1
fi

# Controllo se sono già state applicate le modifiche corrette a vdens.c (cerco se sono già presenti i commenti che andrei a fare)
# nota: non posso fare controlli su .git in quanto sembra che dopo il pull non vi sia alcuna dir .git
if grep -q '/\*uid_gid_map(' "$vdens_c_path" && grep -q '/\*CLONE_NEWUSER' "$vdens_c_path"; then
    echo "From modifyVdens.sh: The modifications to $vdens_c_path have already been applied."
    exit 0
fi

# Modifica il file vdens.c per commentare le chiamate a uid_gid_map e il flag CLONE_NEWUSER.
sed -i 's,\(uid_gid_map(.*);\),/*&*/,' "$vdens_c_path"
if [ $? -ne 0 ]; then
    echo "Error: From modifyVdens.sh: Failed to comment out uid_gid_map calls in $vdens_c_path"
    exit 1
fi

sed -i 's,CLONE_NEWUSER[[:space:]]*|,/*&*/,g' "$vdens_c_path"
if [ $? -ne 0 ]; then
    echo "Error: From modifyVdens.sh: Failed to comment out CLONE_NEWUSER in $vdens_c_path"
    exit 1
fi

echo "From modifyVdens.sh: Successfully modified $vdens_c_path to comment out uid_gid_map calls and CLONE_NEWUSER flag."
exit 0