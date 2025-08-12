# sshlirpCI

**Continuous Integration for sshlirp - Instant VPN**

## Introduction

sshlirpCI was created to bring to life an automated Continuous Integration system for the sshlirp - Instant VPN project.
Indeed, sshlirpCI aims to provide sshlirp binaries that are always updated to the latest source code version and compiled for various architectures.

The project is based on the operation of a daemon that, at regular intervals, retrieves the latest available version of sshlirp and performs cross-compilation for the specified architectures in parallel.

The entire program is designed to log every single operation performed by both the main process and the threads to files, so that in case of warnings, errors, crashes, or even for pure monitoring purposes, the user can know the process status at any time.

The daemon's operation is ultimately based on user-modifiable variables contained in the `ci.conf` configuration file. These variables define the architectures for which cross-compilation is to be performed, the path of the target directory for the final executables, the log file paths, the paths of sshlirpCI's "working" directories, the URL for cloning sshlirp, and the time interval between binary update cycles.
This flexibility could, in the future, allow this project to be expanded into a CI system that also works for other developing projects (besides sshlirp) with some non-substantial modifications to the sshlirpCI code.

## Prerequisites

### Debootstrap

`sshlirp_ci_start` (the daemon startup executable obtained by compiling sshlirpCI - see [Compilation](#compilation) section), as mentioned, relies on debootstrap to perform cross-compilation between the host machine and the target architectures. A fundamental prerequisite is therefore to install this package by running the following commands on your host:

```sh
apt update
apt upgrade
apt install debootstrap
```

### AppArmor restrictions on Ubuntu >= 24.04

In case the user wishes to run the sshlirpCI program on a host with Ubuntu 24.04 or later, it is necessary to change an AppArmor kernel configuration.
In the latest Ubuntu versions, AppArmor applies stricter restrictions to security profiles, adding (by default) checks on the creation of user namespaces by unprivileged users/processes.
Since sshlirpCI aims to complete the debootstrap phase for cross-compilation without requiring elevated privileges, the program, instead of using chroot, adopts a user-namespace-based approach.
Therefore, if you intend to run sshlirpCI with `sudo` (see section [Permissions](#permissions)) nothing else is required; but if you want to run it unprivileged, you must first modify the AppArmor configuration by running the following command:

```sh
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
```

### Dependencies

sshlirpCI mainly relies on standard C libraries and does not use additional libraries.
However, in minimal environments, although unlikely, it might be necessary to install the `build-essential` package.
Also, for a correct clone from the sshlirpCI repo and a working build phase, you will need to install the `git` and `cmake` packages.
Lastly, the `libexecs-dev` package is required since `sshlirp_ci_start` uses `system_safe` instead of `system` to execute the embedded scripts inside the chroot environment.

To install all these packages, run the following command:

```sh
apt install build-essential cmake git libexecs-dev
```

### Permissions

The current version of sshlirpCI, for the sole production of static sshlirp binaries, does not require root privileges.
However, if you also want to use the program to automatically test the produced binaries (see section [Enabling automatic testing for sshlirp binaries](#enabling-automatic-testing-for-sshlirp-binaries)), you must grant sshlirpCI the necessary privileges.
A further scenario in which sshlirpCI may require additional permissions for correct operation is when the user wants to set in the `ci.conf` file a main directory (see section [Changes to variables](#changes-to-variables)) on which their user does not have read/write permissions.
In both cases it is therefore necessary that the user has access both to the `sudo` command and to the `/etc/sudoers` file (which must be modified to ensure that sshlirpCI can execute embedded scripts and launch threads with sufficient privileges).
Alternatively, it is always possible to run sshlirpCI on a virtual machine, where it is certain that the user can meet this requirement.

## Clone

Currently, sshlirpCI is available on a GitHub repository and can be cloned using the following command:

```sh
git clone https://github.com/BBFrank/sshlirpCI.git
```
## Changes to variables

Once the repository is cloned, you will need to modify the only hardcoded path in the code, which is the path that tells the program the location of `ssshlirpCI` directory. The modification must be made in the `src/include/types/types.h` file, replacing line 4 with:

```c
#define DEFAULT_CONFIG_PATH "/path/to/sshlirpCI"
```
    
(where `/path/to/sshlirpCI` is the path where the sshlirpCI repository was cloned)

It will also be necessary to edit the `ci.conf` file to set as you wish the paths of the main and target directories and of the log file, changing the following lines of the configuration file:

```sh
MAIN_DIR=/path/to/main/sshlirpCI
TARGET_DIR=/path/to/main/sshlirpCI/binaries
LOG_FILE=/path/to/main/sshlirpCI/log/main_sshlirp.log
```
(where `/path/to/main/sshlirpCI` is the path of the main sshlirpCI directory, i.e. the directory in which you want the program to build the root filesystems, clone the sources, perform the testing phase, and place the logs and target binaries)

In this context it is recommended to use absolute paths on which the user has read/write permissions. If you want to proceed differently you must satisfy the permission requirements indicated in the section [Permissions](#permissions), and apply the changes suggested in the section [Modifying permissions](#modifying-permissions---only-for-tests-and-ci.conf-with-privileged-directories).

## Compilation

To compile sshlirpCI, follow these steps:

```sh
cd sshlirpCI
mkdir build
cd build
cmake ..
make
```

This series of commands will create a `build` directory inside the sshlirpCI directory, which will contain the following executables:

- `sshlirp_ci_start`: the executable that starts the sshlirpCI daemon
- `sshlirp_ci_stop`: the executable that stops the sshlirpCI daemon and cleans up temporary files
- `sshlirp_ci_instant_killer`: the executable that forcibly kills the process launched by `sshlirp_ci_start` and cleans temporary files, without guaranteeing that the rootfs setup phases are completed consistently.

## Modifying permissions - only for tests and ci.conf with privileged directories

As specified in the sections [Permissions](#permissions) and [Changes to variables](#changes-to-variables), if you wish to enable automatic testing for the sshlirp binaries produced in the rootfs, or if the user wants to specify in the `ci.conf` file directories on which their user has no read/write permissions, it is necessary, before launching the `sshlirp_ci_start` executable, to modify `/etc/sudoers`, granting that executable root privileges. Consequently, assuming that the `sshlirp_ci_start` binary will be launched with the `sudo` command, it will be necessary to grant superuser privileges to the stop/kill binaries as well.
To do this, simply add the following lines to the aforementioned file:

```
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_start
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_stop
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_instant_killer
```
(where `user` is the username of the user who will run the sshlirpCI program and `/path/to/sshlirpCI` is the path where the sshlirpCI repository was cloned)

## Enabling automatic testing for sshlirp binaries

`sshlirp_ci_start` also allows you to enable automatic testing of the sshlirp binaries generated by the daemon, in order
to verify that the binaries are functional and do not crash when launched. This is done by running a test script, which uses
`vdens`, inside the chroot environment of each architecture, which is executed after the compilation phase.
`vdens` however requires access to the network devices in `/dev/net/tun` and these are not set up during the debootstrap phase of the rootfs.
The existing code, in fact, mounts the corresponding host network devices into the chroot filesystem or (depending on the implementation shown in the test.sh and rootlessDebootstrapWrapper.sh files), creates them with mknod at test time. Both of these operations, however, require superuser privileges on the host system.
Therefore, for the test embedded script to work correctly, you must satisfy the privilege prerequisites specified in the section [Permissions](#permissions) and modify `/etc/sudoers`, as indicated in the section [Modifying permissions](#modifying-permissions---only-for-tests-and-ciconf-with-privileged-directories).
It will also be necessary to first verify that the `/dev/net/tun` network interface is present on the host system.
To configure it, if it is not already present, simply run the following commands:

```sh
sudo mkdir -p /dev/net
sudo mknod /dev/net/tun c 10 200
sudo chmod 0666 /dev/net/tun
sudo modprobe tun
sudo ip tuntap add dev tap0 mode tap
sudo ip link set tap0 up
```

And to verify that the configuration was successful:

```sh
ip addr show tap0
```

After taking all these precautions you can set this feature by simply modifying the file `src/include/types/types.h`, changing the value of the `TEST_ENABLED` variable to 1 (to enable it) or 0 (to disable it).

```c
#define TEST_ENABLED 1 // Set to 1 to enable testing, 0 to disable
```

## Starting the daemon

To start the daemon, simply run the following command, replacing `/path/to/sshlirpCI` with the path where the sshlirpCI repository was cloned and optionally adding `sudo` if you want to run the program with elevated privileges:

```sh
/path/to/sshlirpCI/build/build/sshlirp_ci_start
```

## Monitoring the daemon - log files

While the daemon is running (i.e., when it is not in a sleep state, waiting for an update to the sshlirp source code), the main process and the threads it launches for each target architecture defined in `ci.conf` log every operation to separate files, which will be merged by the main process only in the final phase.
Therefore, although the user can simply observe the main log file (whose path is saved in the `LOG_FILE` variable in `ci.conf`) at the end of the execution to check for any errors, they might want to monitor the process's progress in real-time.
To do this, it is always possible to consult the individual thread log files during the daemon's execution:

- **Thread log file on the host**: this can be found in the `THREAD_LOG_DIR` directory (a variable saved in the configuration file)
- **Thread log file in the associated chroot**: this can be found in the `MAIN_DIR/${arch}-chroot/THREAD_CHROOT_LOG_FILE` directory

It is important to specify that before the daemon enters the `SLEEPING` state, all log files are merged into `LOG_FILE` and then their content is cleared.
The status and PID of the process, when active, can always be consulted in the `/tmp/sshlirp_ci.state` and `/tmp/sshlirp_ci.pid` files, respectively.

## Stopping the daemon

Stopping the daemon via `sshlirp_ci_stop` automatically terminates the daemon process and deletes the temporary files created during execution.
To stop the daemon, simply run the following command, replacing `/path/to/sshlirpCI` with the path where the sshlirpCI repository was cloned and adding `sudo` if the start binary was launched similarly:

```sh
/path/to/sshlirpCI/build/build/sshlirp_ci_stop
```

## Forced daemon kill

Since `sshlirp_ci_stop` ensures that the daemon enters the `SLEEPING` state before terminating it (whose `WORKING` state, on the other hand, may last several tens of minutes), you may want to forcibly stop the program. For this purpose, you can use the `sshlirp_ci_instant_killer` command, which will immediately terminate the daemon process and clean temporary files, without guaranteeing that the rootfs setup phases leave the filesystems in a consistent state.
To forcibly kill the daemon, simply run the following command, replacing `/path/to/sshlirpCI` with the path where the sshlirpCI repository was cloned and adding `sudo` if the start binary was launched similarly:

```sh
/path/to/sshlirpCI/build/build/sshlirp_ci_instant_killer
```