#!/bin/bash

sshlirp_bin_path=$1     # nota: sshlirp_bin_path è un percorso relativo al chroot
chroot_path=$2
thread_chroot_vdens_dir=$3
host_log_file=$4
chroot_log_file=$5

absolute_chroot_vdens_dir="${chroot_path}/${thread_chroot_vdens_dir}"

# Controllo se i parametri sono stati passati
if [ -z "$sshlirp_bin_path" ] || [ -z "$chroot_path" ] || [ -z "$thread_chroot_vdens_dir" ] || [ -z "$host_log_file" ] || [ -z "$chroot_log_file" ]; then
    echo "Usage: $0 <sshlirp_bin_path> <chroot_path> <thread_chroot_vdens_dir> <host_log_file> <chroot_log_file>"
    exit 1
fi

# Controllo che il file di log sull'host esista
if [ ! -f $host_log_file ]; then
    echo "Error: From test.sh: Host logfile does not exist."
    exit 1
fi

# Reindirizzo gli output dei comandi e gli echo nel file di log
exec >> $host_log_file 2>&1

# Controlla che il path di chroot esista
if [ ! -d "$chroot_path" ]; then
    echo "Error: From test.sh: Chroot path does not exist at ${chroot_path}"
    exit 1
fi

# Configuro il device TUN necessario per il funzionamento di vdens
echo "Configuring TUN device for vdens..."
if [ ! -c /dev/net/tun ]; then
    echo "Error: From test.sh: /dev/net/tun does not exist on the host."
    exit 1
fi

# Ottengo i numeri major e minor per /dev/net/tun
read MAJOR MINOR < <(ls -l /dev/net/tun | awk '{gsub(",",""); print $5, $6}')
if [ -z "$MAJOR" ] || [ -z "$MINOR" ]; then
    echo "Error: From test.sh: Unable to obtain major/minor numbers for /dev/net/tun."
    exit 1
fi

# Creo la directory /dev/net nel chroot (se ancora non esiste)
sudo mkdir -p "${chroot_path}/dev/net"
if [ ! -d "${chroot_path}/dev/net" ]; then
    echo "Error: From test.sh: Unable to create directory /dev/net in chroot."
    exit 1
fi

# Crea il device TUN nel chroot con i numeri major e minor ottenuti se non esistono già
if [ ! -c "${chroot_path}/dev/net/tun" ]; then
    echo "Creating TUN device in chroot..."
    sudo mknod "${chroot_path}/dev/net/tun" c $MAJOR $MINOR
    if [ $? -ne 0 ]; then
        echo "Error: From test.sh: Unable to create TUN device in chroot."
        exit 1
    fi
    echo "TUN device created successfully in chroot."
else
    echo "TUN device already exists in chroot."
fi

# Imposta i permessi per il device TUN nel chroot
# Nota: i permessi 666 sono necessari per permettere a vdens di accedere
sudo chmod 666 "${chroot_path}/dev/net/tun"
if [ $? -ne 0 ]; then
    echo "Error: From test.sh: Unable to set permissions for TUN device in chroot."
    exit 1
fi

# Creazione del file di configurazione per vdens (a causa del bug di mount è necessario generare a priori il file di resolv.conf)
echo "Creating resolv.conf in chroot..."
echo "nameserver 9.9.9.9" | sudo tee "${chroot_path}/etc/resolv.conf" > /dev/null

# Entro nel chroot
echo "Starting test script inside chroot..."

# Rendendizzo gli output dei comandi e gli echo nel file di log del chroot
absolute_chroot_log_file_path="${chroot_path}${chroot_log_file}"
if [ ! -f "$absolute_chroot_log_file_path" ]; then
    echo "Warning: From test.sh: Chroot logfile does not exist at ${absolute_chroot_log_file_path}; test inside chroot logs will be saved in the host log file."
else
    exec >> "$absolute_chroot_log_file_path" 2>&1
fi

sudo chroot "$chroot_path" /bin/bash <<EOF

    echo "------- From test.sh (inside chroot): testing sshlirp binary at ${sshlirp_bin_path} inside ${chroot_path} -------"

    # Controlla l'esistenza del sshlirp_bin_path di sshlirp (è un percorso relativo al chroot)
    if [ ! -f "$sshlirp_bin_path" ]; then
        echo "Error: From test.sh: sshlirp binary does not exist at ${sshlirp_bin_path}"
        exit 1
    fi

    echo "From test.sh (inside chroot): Installing dependencies..."
    apt-get update
    apt-get install -y libcap-dev libexecs-dev
    if [ \$? -ne 0 ]; then
        echo "Error: From test.sh (inside chroot): Failed to install dependencies."
        exit 1
    fi

    # Controlla l'esistenza della repo vdens all'interno del chroot
    if [ ! -d "$thread_chroot_vdens_dir" ]; then
        echo "Error: From test.sh (inside chroot): vdens directory does not exist at ${thread_chroot_vdens_dir}"
        exit 1
    fi

    echo "From test.sh (inside chroot): Compiling vdens..."
    cd "$thread_chroot_vdens_dir"
    mkdir -p build
    cd build
    cmake ..
    make
    if [ \$? -ne 0 ]; then
        echo "Error: From test.sh (inside chroot): Compilation of vdens failed."
        exit 1
    fi

    # Avvia vdens, che esegue sshlirp, che a sua volta esegue una shell.
    # I comandi seguenti vengono eseguiti in quella shell, nel namespace corretto.
    echo "From test.sh (inside chroot): Entering vdens namespace to run tests..."
    ./vdens cmd://$sshlirp_bin_path /bin/bash <<'INNER_EOF'
        set -e

        # Ora siamo nel namespace di rete creato da vdens
        echo "From test.sh (in vdens namespace): Configuring network... (actual configuration:)"
        ip a

        echo "From test.sh (in vdens namespace): Setting IP address and bringing up vde0..."
        ip addr add 10.0.2.15/24 dev vde0
        if [ \$? -ne 0 ]; then
            echo "Error: From test.sh (in vdens namespace): Unable to set IP address on vde0."
            exit 1
        fi

        ip link set vde0 up
        if [ \$? -ne 0 ]; then
            echo "Error: From test.sh (in vdens namespace): Unable to bring up vde0."
            exit 1
        fi
        echo "From test.sh (in vdens namespace): Network configured successfully:"
        ip a

        echo "From test.sh (in vdens namespace): Pinging gateway through sshlirp..."
        ping -c 4 10.0.2.2
        if [ \$? -ne 0 ]; then
            echo "Error: From test.sh (in vdens namespace): Ping to gateway failed."
            exit 1
        fi
        echo "From test.sh (in vdens namespace): Ping successful."

        exit 0
INNER_EOF

    if [ \$? -ne 0 ]; then
        echo "Error: From test.sh (inside chroot): Script inside vdens namespace failed."
        exit 1
    fi

    echo "------- From test.sh (inside chroot): successful testing inside ${chroot_path} -------"

    exit 0
EOF

# Ripristino del log file dell'host
exec >> $host_log_file 2>&1

if [ $? -ne 0 ]; then
    echo "Error: From test.sh: Script inside chroot failed."
    exit 1
fi

echo "...From test.sh (inside chroot): test script executed successfully."
exit 0