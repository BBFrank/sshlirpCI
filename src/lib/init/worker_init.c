#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "init/worker_init.h"
#include "scripts/chroot_setup_script.h"
#include "scripts/copy_source_script.h"
#include "scripts/compile_script.h"
#include "scripts/remove_source_copy_script.h"
#include "scripts/modify_vdens_script.h"
#include "utils/utils.h"

// Function to configure the chroot for the thread (creates the chroot directory, executes the chroot setup script)
int setup_chroot(thread_args_t* args, FILE* thread_log_fp) {
    
    if (mkdir(args->chroot_path, 0755) == -1 && errno != EEXIST) {
        fprintf(thread_log_fp, "[Thread %s] Failed to create chroot directory %s: %s\n", args->arch, args->chroot_path, strerror(errno));
        return 1;
    }

    // Execute the chroot setup script
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        chroot_setup_script_content,
        "chroot_setup",
        args->arch,
        args->chroot_path,
        args->thread_log_file,
        NULL, NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Chroot setup script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

// Function that checks (and if necessary creates) the worker's directories inside the chroot and its log files (inside and outside the chroot):
// - thread_chroot_main_dir: main directory of the thread inside the chroot (e.g. <path2chroot>/home/sshlirpCI/)
// - thread_chroot_sshlirp_dir: sshlirp directory inside the chroot (e.g. <path2chroot>/home/sshlirpCI/sshlirp)
// - thread_chroot_libslirp_dir: libslirp directory inside the chroot (e.g. <path2chroot>/home/sshlirpCI/libslirp)
// - thread_chroot_target_dir: destination directory for compiled binaries inside the chroot (e.g. <path2chroot>/home/sshlirpCI/thread-binaries)
// - getparent(thread_chroot_log_file): log directory of the thread inside the chroot (e.g. <path2chroot>/home/sshlirpCI/log)
// - thread_chroot_log_file: log file of the thread inside the chroot (e.g. <path2chroot>/home/sshlirpCI/log/thread_sshlirp.log)
int check_worker_dirs(thread_args_t* args, FILE* thread_log_fp) {
    // ex: <path2chroot>/home/sshlirpCI/
    char path_buffer[1024];
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_main_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(thread_log_fp, "[Thread %s] Failed to create main directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // ex: <path2chroot>/home/sshlirpCI/sshlirp
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_sshlirp_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(thread_log_fp, "[Thread %s] Failed to create sshlirp directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // ex: <path2chroot>/home/sshlirpCI/libslirp
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_libslirp_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(thread_log_fp, "[Thread %s] Failed to create libslirp directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // ex: <path2chroot>/home/sshlirpCI/thread_binaries
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_target_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(thread_log_fp, "[Thread %s] Failed to create target directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // ex: <path2chroot>/home/sshlirpCI/log
    char* log_parent_dir_rel = get_parent_dir(args->thread_chroot_log_file);
    if (!log_parent_dir_rel) {
        fprintf(thread_log_fp, "[Thread %s] Failed to get parent directory for chroot log file\n", args->arch);
        return 1;
    }
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, log_parent_dir_rel);
    free(log_parent_dir_rel);

    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(thread_log_fp, "[Thread %s] Failed to create log directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }

    // ex: <path2chroot>/home/sshlirpCI/log/thread_sshlirp.log
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_log_file);
    FILE* thread_chroot_log_fp = fopen(path_buffer, "a");
    if (!thread_chroot_log_fp) {
        fprintf(thread_log_fp, "[Thread %s] Failed to open thread log file inside chroot %s: %s\n", args->arch, path_buffer, strerror(errno));
        return 1;
    }
    fclose(thread_chroot_log_fp);

    return 0;
}

// Function that copies the sshlirp and libslirp sources into the chroot from the host directories
int copy_sources_to_chroot(thread_args_t* args, FILE* thread_log_fp) {
    // Execute the script to copy sources into the chroot (I don't perform the actual chroot yet) for sshlirp
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        copy_source_script_content,
        "copy_sources",
        args->sshlirp_host_source_dir,
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_log_file,
        NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Copy sources script failed for sshlirp with status: %d\n", args->arch, script_status);
        return 1;
    }

    // Now for libslirp
    script_status = execute_embedded_script_for_thread(
        args->arch,
        copy_source_script_content,
        "copy_sources",
        args->libslirp_host_source_dir,
        args->chroot_path,
        args->thread_chroot_libslirp_dir,
        args->thread_log_file,
        NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Copy sources script failed for libslirp with status: %d\n", args->arch, script_status);
        return 1;
    }

    // If testing is enabled, also copy vdens, modifying it on the host first
#ifdef TEST_ENABLED
    
    char vdens_c_path[MAX_CONFIG_ATTR_LEN + 10];

    snprintf(vdens_c_path, sizeof(vdens_c_path), "%s/vdens.c", args->vdens_host_source_dir);
    fprintf(thread_log_fp, "Modifying file %s to disable namespaces...\n", vdens_c_path);

    // Execute the script to modify the vdens.c file to disable namespaces (they cause errors in the chroot)
    script_status = execute_embedded_script_for_thread(
        args->arch,
        modify_vdens_script_content, 
        "modify_vdens", 
        vdens_c_path, 
        args->thread_log_file, 
        NULL, NULL, NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "Error: Error modifying vdens.c file in %s. Script exit status: %d\n", vdens_c_path, script_status);
        return 1;
    }

    // Execute the copy script
    script_status = execute_embedded_script_for_thread(
        args->arch,
        copy_source_script_content,
        "copy_sources",
        args->vdens_host_source_dir,
        args->chroot_path,
        args->thread_chroot_vdens_dir,
        args->thread_log_file,
        NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Copy sources script failed for vdens with status: %d\n", args->arch, script_status);
        return 1;
    }

#endif

    return 0;
}

// Function that compiles and verifies the sshlirp sources inside the chroot (when I run the script I will actually enter the chroot)
int compile_and_verify_in_chroot(thread_args_t* args, FILE* thread_log_fp) {
    // Execute the compilation script inside the chroot
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        compile_script_content,
        "compile_sshlirp",
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_chroot_libslirp_dir,
        args->thread_chroot_target_dir,
        args->arch,
        args->thread_chroot_log_file,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Compile script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

int remove_sources_copy_from_chroot(thread_args_t* args, FILE* thread_log_fp) {
    // Execute the script to remove sources inside the chroot
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        remove_source_copy_script_content,
        "remove_sources_copy",
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_chroot_libslirp_dir,
        args->thread_log_file,
        NULL, NULL,
        thread_log_fp
    );

    if (script_status != 0) {
        fprintf(thread_log_fp, "[Thread %s] Remove sources script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

