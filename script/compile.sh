#!/bin/bash

chroot_path=$1
sshlirp_chroot_src_dir=$2
libslirp_chroot_src_dir=$3
target_chroot_dir=$4
arch=$5
chroot_logfile=$6

# Controllo che i parametri siano stati passati
if [ -z "$chroot_path" ] || [ -z "$sshlirp_chroot_src_dir" ] || [ -z "$libslirp_chroot_src_dir" ] || [ -z "$target_chroot_dir" ] || [ -z "$arch" ] || [ -z "$chroot_logfile" ]; then
    echo "From compile.sh: Usage: $0 <chroot_path> <sshlirp_chroot_src_dir> <libslirp_chroot_src_dir> <target_chroot_dir> <arch> <chroot_logfile>"
    exit 1
fi

# Ottengo il path assoluto del chroot log file (mi serve solo per il primo eventuale log)
abs_chroot_log_file_path="$chroot_path$chroot_logfile"

# Controllo che il file di log esista
if [ ! -f "$abs_chroot_log_file_path" ]; then
    echo "Error: From compile.sh: Logfile does not exist: $abs_chroot_log_file_path"
    exit 1
fi

# Reindirizzo gli output dei comandi e gli echo nel file di log (questa volta è il file di log interno al chroot)
exec >> "$abs_chroot_log_file_path" 2>&1

# Controllo che il chroot esista
if [ ! -d "$chroot_path" ]; then
    echo "Error: From compile.sh: Chroot path $chroot_path does not exist."
    exit 1
fi

# Entro nel chroot avviando una shell che prende in input il codice dell'here document (EOF...EOF)
# Nota: è necessario fare ciò in quanto il comportamento di default di chroot è di aprire una shell interattiva, e basta ->
# il processo padre quindi attenderebbe indefinitamente la chiusura della shell, che non avverrebbe mai, in quanto i comandi specificati
# dopo il chroot sono da eseguire nell'host.
# In questo modo invece è come se si desse in input alla shell del chroot un comando da eseguire.
# Nota: questa non è la soluzione più elegante perchè comunque viene passato uno script grande e complesso come linea di comando alla nuova shell...
# L'ideale sarebbe creare uno script di shell separato e passarlo come argomento a chroot...
sudo chroot "$chroot_path" /bin/bash <<EOF

# Controlla se la directory in cui metterò i binari nel chroot esiste
if [ ! -d "$target_chroot_dir" ]; then
    echo "Error: From compile.sh (inside chroot): Target chroot directory $target_chroot_dir does not exist in $arch chroot."
    exit 1
fi

# Installo le dipendenze necessarie per la compilazione
echo "From compile.sh (inside chroot): Installing build dependencies..."
apt-get update
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to update package list."
    exit 1
fi
apt install -y build-essential git devscripts debhelper dh-exec \\
            libglib2.0-dev pkg-config \\
            gcc g++ libcap-ng-dev libseccomp-dev \\
	        cmake git-buildpackage meson ninja-build \\
	        libvdeplug-dev libvdeslirp-dev

if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to install build dependencies."
    exit 1
fi

# Mi sposto nella directory di libslirp source dentro il chroot
cd "$libslirp_chroot_src_dir"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to change directory to $libslirp_chroot_src_dir."
    exit 1
fi

# Compilo libslirp
echo "From compile.sh (inside chroot): Compiling libslirp..."
meson setup build --default-library=static
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to set up meson build for libslirp."
    exit 1
fi
ninja -C build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to build libslirp."
    exit 1
fi
ninja -C build install
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to install libslirp."
    exit 1
fi
rm -rf build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to remove build directory for libslirp."
    exit 1
fi
echo "From compile.sh (inside chroot): libslirp compiled and installed successfully."

# Mi sposto nella directory di sshlirp source dentro il chroot
cd "$sshlirp_chroot_src_dir"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to change directory to $sshlirp_chroot_src_dir."
    exit 1
fi

# Compilo sshlirp
echo "From compile.sh (inside chroot): Compiling sshlirp..."

# Se sono su trixie devo prima modificare il CMakeLists.txt per correggere l'importazione dei simboli
# di sysprof (aggiungere fake.c - se esistente nella directory di sshlirp source - nell'add_executable(sshlirp sshlirp.c libvdeslirp.c libvdestream.c autoopt.c),
# che quindi deve diventare: add_executable(sshlirp sshlirp.c libvdeslirp.c libvdestream.c autoopt.c fake.c)
if [ "$arch" != "arm64" ]; then          <-- modificato da specifiche => utilizzare trixie per tutte le arches (non si può usare su arm64 in quanto trixie - unstable - blocca l'unpacking del base system su arm64)
    echo "From compile.sh (inside chroot): Modifying CMakeLists.txt for trixie release..."
    # Controllo se fake.c esiste
    if [ -f "fake.c" ]; then
        echo "From compile.sh (inside chroot): fake.c found, modifying CMakeLists.txt..."
        sed -i 's/add_executable(sshlirp sshlirp.c libvdeslirp.c libvdestream.c autoopt.c)/add_executable(sshlirp sshlirp.c libvdeslirp.c libvdestream.c autoopt.c fake.c)/' CMakeLists.txt
        if [ \$? -ne 0 ]; then
            echo "Error: From compile.sh (inside chroot): Failed to modify CMakeLists.txt."
            exit 1
        fi
    else
        echo "Warning: From compile.sh (inside chroot): fake.c not found, skipping modification of CMakeLists.txt."
    fi
fi

# Creo la directory di build e mi sposto in essa
mkdir -p build
cd build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to create and change directory to build."
    exit 1
fi

# Configuro il progetto con CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$target_chroot_dir" -DCMAKE_EXE_LINKER_FLAGS="-static"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to configure CMake for sshlirp."
    exit 1
fi

# Compilo il progetto
make
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to build sshlirp."
    exit 1
fi

# Installo il progetto
make install
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to install sshlirp."
    exit 1
fi

# Rimuovo la directory di build
cd ..
rm -rf build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to remove build directory for sshlirp."
    exit 1
fi

# Verifico che il binario sia stato installato correttamente e che sia un eseguibile statico. Infine lo
# rinomino come sshlirp-<arch>
if [ ! -f "$target_chroot_dir/bin/sshlirp" ]; then
    echo "Error: From compile.sh (inside chroot): Expected binary sshlirp not found in $target_chroot_dir/bin."
    exit 1
fi
if ! file "$target_chroot_dir/bin/sshlirp" | grep -q "statically linked"; then
    echo "Error: From compile.sh (inside chroot): $target_chroot_dir/bin/sshlirp is not a statically linked executable."
    exit 1
fi
mv "$target_chroot_dir/bin/sshlirp" "$target_chroot_dir/bin/sshlirp-$arch"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to rename sshlirp binary to sshlirp-$arch."
    exit 1
fi

echo "From compile.sh (inside chroot): sshlirp compiled and installed successfully as sshlirp-$arch in $target_chroot_dir/bin."
exit 0
EOF

if [ $? -ne 0 ]; then
    echo "Error: From compile.sh: Script inside chroot failed."
    exit 1
fi

echo "From compile.sh: Compilation process finished successfully."
exit 0
