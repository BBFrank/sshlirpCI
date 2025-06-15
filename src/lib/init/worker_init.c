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
#include "utils/utils.h"

// Funzione per configurare il chroot per il thread (crea la directory di chroot, esegue lo script di setup del chroot)
int setup_chroot(thread_args_t* args) {
    
    if (mkdir(args->chroot_path, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "[Thread %s] Failed to create chroot directory %s: %s\n", args->arch, args->chroot_path, strerror(errno));
        return 1;
    }

    // Eseguo lo script di setup del chroot
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        chroot_setup_script_content,
        "chroot_setup",
        args->arch,
        args->chroot_path,
        args->thread_log_file,
        NULL, NULL, NULL
    );

    if (script_status != 0) {
        fprintf(stderr, "[Thread %s] Chroot setup script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

// Funzione che si occupa di controllare (e nel caso creare) le directory del worker dentro il chroot e i suoi file di log (dentro e fuori il chroot):
// - thread_chroot_main_dir: directory principale del thread dentro il chroot (es: <path2chroot>/home/sshlirpCI/)
// - thread_chroot_sshlirp_dir: directory di sshlirp dentro il chroot (es: <path2chroot>/home/sshlirpCI/sshlirp)
// - thread_chroot_libslirp_dir: directory di libslirp dentro il chroot (es: <path2chroot>/home/sshlirpCI/libslirp)
// - thread_chroot_target_dir: directory di destinazione dei binari compilati dentro il chroot (es: <path2chroot>/home/sshlirpCI/thread-binaries)
// - getparent(thread_chroot_log_file): directory di log del thread dentro il chroot (es: <path2chroot>/home/sshlirpCI/log)
// - thread_chroot_log_file: file di log del thread dentro il chroot (es: <path2chroot>/home/sshlirpCI/log/thread_sshlirp.log)
int check_worker_dirs(thread_args_t* args) {
    // es: <path2chroot>/home/sshlirpCI/
    char path_buffer[1024];
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_main_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(stderr, "[Thread %s] Failed to create main directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // es: <path2chroot>/home/sshlirpCI/sshlirp
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_sshlirp_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(stderr, "[Thread %s] Failed to create sshlirp directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // es: <path2chroot>/home/sshlirpCI/libslirp
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_libslirp_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(stderr, "[Thread %s] Failed to create libslirp directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // es: <path2chroot>/home/sshlirpCI/thread_binaries
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_target_dir);
    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(stderr, "[Thread %s] Failed to create target directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }
    // es: <path2chroot>/home/sshlirpCI/log
    char* log_parent_dir_rel = get_parent_dir(args->thread_chroot_log_file);
    if (!log_parent_dir_rel) {
        fprintf(stderr, "[Thread %s] Failed to get parent directory for chroot log file\n", args->arch);
        return 1;
    }
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, log_parent_dir_rel);
    free(log_parent_dir_rel);

    if(access(path_buffer, F_OK) == -1) {
        if (mkdir(path_buffer, 0755) == -1) {
            fprintf(stderr, "[Thread %s] Failed to create log directory inside chroot: %s\n", args->arch, strerror(errno));
            return 1;
        }
    }

    // es: <path2chroot>/home/sshlirpCI/log/thread_sshlirp.log
    snprintf(path_buffer, sizeof(path_buffer), "%s%s", args->chroot_path, args->thread_chroot_log_file);
    FILE* thread_chroot_log_fp = fopen(path_buffer, "a");
    if (!thread_chroot_log_fp) {
        fprintf(stderr, "[Thread %s] Failed to open thread log file inside chroot %s: %s\n", args->arch, path_buffer, strerror(errno));
        return 1;
    }
    fclose(thread_chroot_log_fp);

    return 0;
}

// Funzione che si occupa di copiare i sorgenti di sshlirp e libslirp dentro il chroot dalle directory host
int copy_sources_to_chroot(thread_args_t* args) {
    // Eseguo lo script di copia dei sorgenti dentro il chroot (non effettuo ancora il chroot effettivo)
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        copy_source_script_content,
        "copy_sources",
        args->sshlirp_host_source_dir,
        args->libslirp_host_source_dir,
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_chroot_libslirp_dir,
        args->thread_log_file
    );

    if (script_status != 0) {
        fprintf(stderr, "[Thread %s] Copy sources script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

// Funzione che si occupa di compilare e verificare i sorgenti di sshlirp dentro il chroot (quando lancio lo script entrerò effettivamente nel chroot)
int compile_and_verify_in_chroot(thread_args_t* args) {
    // Eseguo lo script di compilazione dentro il chroot
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        compile_script_content,
        "compile_sshlirp",
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_chroot_libslirp_dir,
        args->thread_chroot_target_dir,
        args->arch,
        args->thread_chroot_log_file
    );

    if (script_status != 0) {
        fprintf(stderr, "[Thread %s] Compile script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

int remove_sources_copy_from_chroot(thread_args_t* args) {
    // Eseguo lo script di rimozione dei sorgenti dentro il chroot
    int script_status = execute_embedded_script_for_thread(
        args->arch,
        remove_source_copy_script_content,
        "remove_sources",
        args->chroot_path,
        args->thread_chroot_sshlirp_dir,
        args->thread_chroot_libslirp_dir,
        args->thread_log_file,
        NULL, NULL
    );

    if (script_status != 0) {
        fprintf(stderr, "[Thread %s] Remove sources script failed with status: %d\n", args->arch, script_status);
        return 1;
    }

    return 0;
}

