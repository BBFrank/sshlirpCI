#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>

#define DEFAULT_CONFIG_PATH "/home/francesco/Scrivania/terzo_anno/Sistemi_Virtuali/sshlirp/sshlirpCI/sshlirpCI/ci.conf"
#define CONFIG_SSHLIRP_KEY "SSHLIRP_REPO_URL="
#define CONFIG_LIBSLIRP_KEY "LIBSLIRP_REPO_URL="
#define CONFIG_MAINDIR_KEY "MAIN_DIR="
#define CONFIG_VERSION_FILE_KEY "VERSIONING_FILE="
#define CONFIG_TARGETDIR_KEY "TARGET_DIR="
#define CONFIG_SSHLIRP_SOURCE_DIR_KEY "SSHLIRP_SOURCE_DIR="
#define CONFIG_LIBSLIRP_SOURCE_DIR_KEY "LIBSLIRP_SOURCE_DIR="
#define CONFIG_LOG_KEY "LOG_FILE="
#define CONFIG_THREAD_LOG_DIR_KEY "THREAD_LOG_DIR="
#define CONFIG_THREAD_CHROOT_TARGET_DIR_KEY "THREAD_CHROOT_TARGET_DIR="
#define CONFIG_THREAD_CHROOT_LOG_FILE_KEY "THREAD_CHROOT_LOG_FILE="
#define CONFIG_INTERVAL_KEY "POLL_INTERVAL="
#define CONFIG_ARCH_KEY "ARCHITECTURES="

#define MAX_ARCHITECTURES 9
#define MAX_CONFIG_LINE_LEN 512
#define MAX_CONFIG_ATTR_LEN 256
#define MAX_COMMAND_LEN 2048
#define MAX_VERSIONING_LINE_LEN 128

typedef struct {
    int pull_round;
    char arch[16];
    char sshlirp_host_source_dir[MAX_CONFIG_ATTR_LEN];
    char libslirp_host_source_dir[MAX_CONFIG_ATTR_LEN];
    char chroot_path[MAX_CONFIG_ATTR_LEN];
    char thread_chroot_main_dir[MAX_CONFIG_ATTR_LEN];
    char thread_chroot_sshlirp_dir[MAX_CONFIG_ATTR_LEN];
    char thread_chroot_libslirp_dir[MAX_CONFIG_ATTR_LEN];
    char thread_chroot_target_dir[MAX_CONFIG_ATTR_LEN];
    char thread_chroot_log_file[MAX_CONFIG_ATTR_LEN];
    char thread_log_file[MAX_CONFIG_ATTR_LEN];
    pthread_mutex_t *chroot_setup_mutex;
} thread_args_t;

typedef struct {
    int status;
    char *new_release;
} commit_status_t;

typedef struct {
    int status;
    char *error_message;
} thread_result_t;

#endif // TYPES_H