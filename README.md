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

### Qemu static user and debootstrap

`sshlirp_ci_start` (the daemon startup executable obtained by compiling sshlirpCI - see [Compilation](#compilation) section), as mentioned, relies on qemu static user to perform cross-compilation between the host machine and the target architectures, and to do this, it uses debootstrap. A fundamental prerequisite is therefore to install these packages by running the following commands on your host:

```sh
sudo apt update
sudo apt upgrade
sudo apt install debootstrap qemu-user-static
```

Furthermore, for the execution of the embedded scripts (generated by the daemon itself, whose content can be viewed in the `script` folder) to work, the user must have configured `apt` to install the required packages to the `/usr/bin/` path.

### Dependencies

sshlirpCI mainly relies on standard C libraries and does not use additional libraries.
However, in minimal environments, although unlikely, it might be necessary to install the `build-essential` package.
Also, for a correct clone from the sshlirpCI repo and a working build phase, you will need to install the `git` and `cmake` packages.
Lastly, the `libexecs-dev` package is required since `sshlirp_ci_start` uses `system_safe` instead of `system` to execute the embedded scripts inside the chroot environment.

To install all these packages, run the following command:

```sh
sudo apt install build-essential cmake git libexecs-dev
```

### Permissions

To correctly run sshlirpCI, the user must have root privileges and therefore access to the sudo command, as well as to the `/etc/sudoers` file (which will need to be modified to grant root privileges to the compilation threads as well).
Alternatively, it is always possible to run sshlirpCI on a virtual machine, where it is certain that the user can meet this requirement.

## Clone

Currently, sshlirpCI is available on a GitHub repository and can be cloned using the following command:

```sh
git clone https://github.com/BBFrank/sshlirpCI.git
```

Once the repository is cloned, you will need to modify the only hardcoded path in the code, which is the path that tells the program the location of `ci.conf`. The modification must be made in the `src/include/types/types.h` file, replacing line 6 with:

```c
#define DEFAULT_CONFIG_PATH "/path/to/sshlirpCI/ci.conf"
```
    
(where `/path/to/sshlirpCI` is the path where the sshlirpCI repository was cloned)

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

## Modifying permissions

As a direct consequence of using debootstrap (which can only run as root) by the `sshlirp_ci_start` executable and due to operations requiring root privileges by both `sshlirp_ci_start` and `sshlirp_ci_stop`, before launching the start executable, you must modify the `/etc/sudoers` file so that the two executables, when launched with the `sudo` command, can have root privileges. To do this, simply add the following lines to the aforementioned file:

```
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_start
user ALL=(root) NOPASSWD: /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```
## Enabling automatic testing for sshlirp binaries

`sshlirp_ci_start` also allows you to enable automatic testing of the sshlirp binaries generated by the daemon, in order
to verify that the binaries are functional and do not crash when launched. This is done by running a test script, which uses
vdens, inside the chroot environment of each architecture, which is executed after the compilation phase.
You can set this feature by simply modifying the file `src/include/types/types.h`, changing the value of the `TEST_ENABLED` variable to 1 (to enable it) or 0 (to disable it).

```c
#define TEST_ENABLED 1 // Set to 1 to enable testing, 0 to disable
```

## Starting the daemon

To start the daemon, simply run the following command, replacing `/path/to/sshlirpCI` with the path where the sshlirpCI repository was cloned:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_start
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
To stop the daemon, simply run the following command, replacing `/path/to/sshlirpCI` with the path where the sshlirpCI repository was cloned:

```sh
sudo /path/to/sshlirpCI/build/build/sshlirp_ci_stop
```
---
