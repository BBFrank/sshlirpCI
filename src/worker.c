#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "init/worker_init.h"
#include "worker.h"
#include "test.h"

void *build_worker(void *arg_ptr) {
    thread_args_t* args = (thread_args_t*)arg_ptr;
    thread_result_t* result = malloc(sizeof(thread_result_t));
    if (!result) {
        pthread_exit(NULL); 
    }
    result->status = 1;
    result->error_message = NULL;

    // Create the thread's log file on the host if it doesn't exist (note: the directory containing all thread log files
    // was created by the main init)
    FILE* thread_log_fp = fopen(args->thread_log_file, "a");
    if (!thread_log_fp) {
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Failed to open thread log file %s: %s", args->thread_log_file, strerror(errno));
        result->error_message = strdup(err_buf);
        pthread_exit(result);
    }

    // Set line buffering for the thread's log file, so that prints are written immediately after each newline
    setvbuf(thread_log_fp, NULL, _IOLBF, 0);

    fprintf(thread_log_fp, "Worker started for arch %s. Pull round: %d.\n", args->arch, args->pull_round);

    if (args->pull_round == 0) {
        fprintf(thread_log_fp, "First run (pull_round 0). Checking and eventually setting up chroot for %s.\n", args->arch);
        
        // This is the only point in the entire program where it is necessary to lock the mutex. This is not done
        // for reasons of concurrent access to shared resources (each thread operates on its "personal" files throughout the process), but for reasons of computational
        // resource consumption. In fact, after several tests, I noticed that the last thread launched terminated the chroot_setup script with status 126 (script found but not executable)
        // because the other threads launched first consumed all available CPU resources, not allowing the last one to execute the chroot_setup script,
        // which is indeed the most expensive operation.
        // The use of this lock therefore guarantees the absence of race conditions, albeit at the expense of total execution time.
        pthread_mutex_lock(args->chroot_setup_mutex);
        int setup_status = setup_chroot(args, thread_log_fp);
        pthread_mutex_unlock(args->chroot_setup_mutex);

        if (setup_status != 0) {
            fprintf(thread_log_fp, "Failed to check/setup chroot for %s.\n", args->arch);
            char err_buf[MAX_CONFIG_ATTR_LEN*2];
            snprintf(err_buf, sizeof(err_buf), "Chroot check/setup failed for %s.", args->arch);
            result->error_message = strdup(err_buf);
            fclose(thread_log_fp);
            pthread_exit(result);
        }
        fprintf(thread_log_fp, "Chroot setup complete for %s.\n", args->arch);

        // The operation of checking/creating the worker's directories inside the chroot can be done without a lock
        fprintf(thread_log_fp, "Checking and eventually creating worker directories and log file inside chroot for %s.\n", args->arch);
        if (check_worker_dirs(args, thread_log_fp) != 0) {
            fprintf(thread_log_fp, "Failed to check/create worker directories for %s.\n", args->arch);
            char err_buf[MAX_CONFIG_ATTR_LEN*2];
            snprintf(err_buf, sizeof(err_buf), "Worker directories check/create failed for %s.", args->arch);
            result->error_message = strdup(err_buf);
            fclose(thread_log_fp);
            pthread_exit(result);
        }
        fprintf(thread_log_fp, "Worker directories and log file checked/created for %s.\n", args->arch);

    } else {
        fprintf(thread_log_fp, "Not first run (pull_round %d). Skipping chroot setup and dir check for %s.\n", args->pull_round, args->arch);
    }

    fprintf(thread_log_fp, "Copying sources into chroot for %s.\n", args->arch);

    // I don't lock this operation with the mutex as I will only be copying the same sshlirp/libslirp source code (read operation)
    if (copy_sources_to_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "Failed to copy sources for %s.\n", args->arch);
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Sources copy failed for %s.", args->arch);
        result->error_message = strdup(err_buf);
        fclose(thread_log_fp);
        pthread_exit(result);
    }
    fprintf(thread_log_fp, "Sources copied for %s.\n", args->arch);

    // Compilation (occurs inside the chroot so logs will go to args->thread_chroot_log_file)
    fprintf(thread_log_fp, "Starting compilation process in chroot for %s...\n", args->arch);
    if (compile_and_verify_in_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "...Compilation process failed for %s. Removing sources copy...\n", args->arch);

        // Deleting source copies even in case of compilation failure
        if (remove_sources_copy_from_chroot(args, thread_log_fp) != 0) {
            fprintf(thread_log_fp, "Error: Failed to remove sources copy for %s.\n", args->arch);
            char err_buf[MAX_CONFIG_ATTR_LEN*2];
            snprintf(err_buf, sizeof(err_buf), "Failed to remove sources copy after compilation failure for %s.", args->arch);
            result->error_message = strdup(err_buf);
            fclose(thread_log_fp);
            pthread_exit(result);
        }
        
        fprintf(thread_log_fp, "Sources copy removed after compilation failure for %s.\n", args->arch);
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Compilation failed for %s.", args->arch);
        result->error_message = strdup(err_buf);
        fclose(thread_log_fp);
        pthread_exit(result);
    }

    fprintf(thread_log_fp, "...Compilation successful for %s.\n", args->arch);

#ifdef TEST_ENABLED
    fprintf(thread_log_fp, "Running tests in chroot for %s...\n", args->arch);
    // Run tests (if enabled) inside the chroot
    char target_chroot_bin_path[MAX_CONFIG_ATTR_LEN*3];
    snprintf(target_chroot_bin_path, sizeof(target_chroot_bin_path), "%s/bin/sshlirp-%s", args->thread_chroot_target_dir, args->arch);
    if (test_sshlirp_bin(args, target_chroot_bin_path, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "...Tests failed for %s.", args->arch);
    }
    else {
        fprintf(thread_log_fp, "...Tests passed for %s.\n", args->arch);
    }
#endif

    fprintf(thread_log_fp, "Removing sources copy for %s.\n", args->arch);

    // Deleting source copies (chroot level operations, does not require mutex)
    if (remove_sources_copy_from_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "Error: Failed to remove sources copy for %s.\n", args->arch);
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Failed to remove sources copy for %s.", args->arch);
        result->error_message = strdup(err_buf);
        fclose(thread_log_fp);
        pthread_exit(result);
    }
    fprintf(thread_log_fp, "Sources copy removed for %s.\n", args->arch);

    fprintf(thread_log_fp, "Worker finished successfully for arch %s.\n", args->arch);

    fclose(thread_log_fp);
    result->status = 0;
    pthread_exit(result);
}