#!/bin/bash

chroot_path=$1
sshlirp_chroot_src_dir=$2
libslirp_chroot_src_dir=$3
target_chroot_dir=$4
arch=$5
chroot_logfile=$6

# Check if parameters were passed
if [ -z "$chroot_path" ] || [ -z "$sshlirp_chroot_src_dir" ] || [ -z "$libslirp_chroot_src_dir" ] || [ -z "$target_chroot_dir" ] || [ -z "$arch" ] || [ -z "$chroot_logfile" ]; then
    echo "From compile.sh: Usage: $0 <chroot_path> <sshlirp_chroot_src_dir> <libslirp_chroot_src_dir> <target_chroot_dir> <arch> <chroot_logfile>"
    exit 1
fi

# Get the absolute path of the chroot log file (needed only for the first potential log)
abs_chroot_log_file_path="$chroot_path$chroot_logfile"

# Check that the log file exists
if [ ! -f "$abs_chroot_log_file_path" ]; then
    echo "Error: From compile.sh: Logfile does not exist: $abs_chroot_log_file_path"
    exit 1
fi

# Redirect command outputs and echoes to the log file (this time it's the log file inside the chroot)
exec >> "$abs_chroot_log_file_path" 2>&1

# Check that the chroot exists
if [ ! -d "$chroot_path" ]; then
    echo "Error: From compile.sh: Chroot path $chroot_path does not exist."
    exit 1
fi

# Enter the chroot by starting a shell that takes the here document code as input (EOF...EOF)
# Note: this is necessary because the default behavior of chroot is to open an interactive shell, and that's it ->
# the parent process would then wait indefinitely for the shell to close, which would never happen, as the commands specified
# after chroot are to be executed on the host.
# This way, it's as if a command to be executed is given as input to the chroot shell.
# Note: this is not the most elegant solution because a large and complex script is still passed as a command line to the new shell...
# The ideal would be to create a separate shell script and pass it as an argument to chroot...
sudo chroot "$chroot_path" /bin/bash <<EOF

# Check if the directory where I will put the binaries in the chroot exists
if [ ! -d "$target_chroot_dir" ]; then
    echo "Error: From compile.sh (inside chroot): Target chroot directory $target_chroot_dir does not exist in $arch chroot."
    exit 1
fi

# Install the dependencies necessary for compilation
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

# Move to the libslirp source directory inside the chroot
cd "$libslirp_chroot_src_dir"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to change directory to $libslirp_chroot_src_dir."
    exit 1
fi

# Compile libslirp
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

# Move to the sshlirp source directory inside the chroot
cd "$sshlirp_chroot_src_dir"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to change directory to $sshlirp_chroot_src_dir."
    exit 1
fi

# Compile sshlirp
echo "From compile.sh (inside chroot): Compiling sshlirp..."

# Create the build directory and move into it
mkdir -p build
cd build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to create and change directory to build."
    exit 1
fi

# Configure the project with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$target_chroot_dir"
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to configure CMake for sshlirp."
    exit 1
fi

# Compile the project
make
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to build sshlirp."
    exit 1
fi

# Install the project
make install
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to install sshlirp."
    exit 1
fi

# Remove the build directory
cd ..
rm -rf build
if [ \$? -ne 0 ]; then
    echo "Error: From compile.sh (inside chroot): Failed to remove build directory for sshlirp."
    exit 1
fi

# Get the system architecture to know what the binary will be called
binary_arch=\$(uname -m)
if [ -z "\$binary_arch" ]; then
    echo "Error: From compile.sh (inside chroot): Could not determine binary architecture using uname -m."
    exit 1
fi

# Verify that the binary was installed correctly and that it is a static executable. Finally, rename it as sshlirp-<arch>
if [ ! -f "$target_chroot_dir/bin/sshlirp-\$binary_arch" ]; then
    echo "Error: From compile.sh (inside chroot): Expected binary sshlirp-\$binary_arch not found in $target_chroot_dir/bin for architecture $arch."
    exit 1
fi
if ! file "$target_chroot_dir/bin/sshlirp-\$binary_arch" | grep -q "statically linked"; then
    echo "Error: From compile.sh (inside chroot): $target_chroot_dir/bin/sshlirp-\$binary_arch is not a statically linked executable."
    exit 1
fi

# Rename the binary to sshlirp-<arch> only if binary_arch is different from arch
if [ "\$binary_arch" != "$arch" ]; then
    mv "$target_chroot_dir/bin/sshlirp-\$binary_arch" "$target_chroot_dir/bin/sshlirp-$arch"
    if [ \$? -ne 0 ]; then
        echo "Error: From compile.sh (inside chroot): Failed to rename sshlirp-\$binary_arch binary to sshlirp-$arch."
        exit 1
    fi
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
