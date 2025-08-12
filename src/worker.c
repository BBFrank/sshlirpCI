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

// Funzione sicura per accumulare le stats evitando overflow con strcat su buffer insufficienti
static int append_stat(thread_result_t *res, const char *text) {
    if (!text) return 0;
    if (!res->stats) {
        res->stats = strdup(text);
        return res->stats ? 0 : -1;
    }
    size_t oldlen = strlen(res->stats);
    size_t addlen = strlen(text);
    char *newbuf = realloc(res->stats, oldlen + addlen + 1);
    if (!newbuf) return -1;
    res->stats = newbuf;
    memcpy(res->stats + oldlen, text, addlen + 1);
    return 0;
}

#define APPEND_STAT_OR_FAIL(msg_literal) \
    if (append_stat(result, msg_literal) != 0) { \
        fprintf(thread_log_fp, "Failed to append stats (%s) for %s.\n", msg_literal, args->arch); \
        result->error_message = strdup("Out of memory while appending stats"); \
        free(result->stats); \
        result->stats = NULL; \
        fclose(thread_log_fp); \
        pthread_exit(result); \
    }

#define APPEND_PROGRESS_STAT() \
    char _stat_buf[MAX_CONFIG_ATTR_LEN]; \
    snprintf(_stat_buf, sizeof(_stat_buf), "Progress: %.2f%%\n", (completed_tasks * 100.0) / total_tasks); \
    APPEND_STAT_OR_FAIL(_stat_buf); \

#define FAIL_AND_EXIT(log_fmt, err_fmt, ...) \
    fprintf(thread_log_fp, log_fmt, ##__VA_ARGS__); \
    char err_buf[MAX_CONFIG_ATTR_LEN*2]; \
    snprintf(err_buf, sizeof(err_buf), err_fmt, ##__VA_ARGS__); \
    result->error_message = strdup(err_buf); \
    APPEND_PROGRESS_STAT(); \
    fclose(thread_log_fp); \
    pthread_exit(result); \

void *build_worker(void *arg_ptr) {
    thread_args_t* args = (thread_args_t*)arg_ptr;
    thread_result_t* result = malloc(sizeof(thread_result_t));
    if (!result) {
        pthread_exit(NULL); 
    }
    result->status = 1;
    result->error_message = NULL;
    result->stats = NULL;
    int total_tasks = 5;
#ifdef TEST_ENABLED
    total_tasks += 1;
#endif
    int completed_tasks = 0;

    // Create the thread's log file on the host if it doesn't exist (note: the directory containing all thread log files
    // was created by the main init)
    FILE* thread_log_fp = fopen(args->thread_log_file, "a");
    if (!thread_log_fp) {
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Failed to open thread log file %s: %s", args->thread_log_file, strerror(errno));
        result->error_message = strdup(err_buf);
        char stat_buf[MAX_CONFIG_ATTR_LEN];
        snprintf(stat_buf, sizeof(stat_buf), "Progress: %.2f%%\n", (completed_tasks * 100.0) / total_tasks);
        result->stats = strdup(stat_buf);
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
            FAIL_AND_EXIT("Failed to check/setup chroot for %s.\n", "Chroot check/setup failed for %s.", args->arch);
        }
        fprintf(thread_log_fp, "Chroot setup complete for %s.\n", args->arch);
        result->stats = strdup("Chroot setup: done\n");
        completed_tasks++;

        // The operation of checking/creating the worker's directories inside the chroot can be done without a lock
        fprintf(thread_log_fp, "Checking and eventually creating worker directories and log file inside chroot for %s.\n", args->arch);
        if (check_worker_dirs(args, thread_log_fp) != 0) {
            FAIL_AND_EXIT("Failed to check/create worker directories for %s.\n", "Worker directories check/create failed for %s.", args->arch);
        }
        fprintf(thread_log_fp, "Worker directories and log file checked/created for %s.\n", args->arch);
        APPEND_STAT_OR_FAIL("Worker directories check/create: done\n");
        completed_tasks++;
    } else {
        fprintf(thread_log_fp, "Not first run (pull_round %d). Skipping chroot setup and dir check for %s.\n", args->pull_round, args->arch);
        result->stats = strdup("Chroot setup: skipped\n");
        completed_tasks = completed_tasks + 2;
    }

    fprintf(thread_log_fp, "Copying sources into chroot for %s.\n", args->arch);

    // I don't lock this operation with the mutex as I will only be copying the same sshlirp/libslirp source code (read operation)
    if (copy_sources_to_chroot(args, thread_log_fp) != 0) {
        FAIL_AND_EXIT("Failed to copy sources for %s.\n", "Sources copy failed for %s.", args->arch);
    }
    fprintf(thread_log_fp, "Sources copied for %s.\n", args->arch);
    APPEND_STAT_OR_FAIL("Sources copy: done\n");
    completed_tasks++;

    // Compilation (occurs inside the chroot so logs will go to args->thread_chroot_log_file)
    fprintf(thread_log_fp, "Starting compilation process in chroot for %s...\n", args->arch);
    if (compile_and_verify_in_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "...Compilation process failed for %s. Removing sources copy...\n", args->arch);
        if (remove_sources_copy_from_chroot(args, thread_log_fp) != 0) {
            FAIL_AND_EXIT("Error: Failed to remove sources copy for %s.\n", "Failed to remove sources copy after compilation failure for %s.", args->arch);
        }
        fprintf(thread_log_fp, "Sources copy removed after compilation failure for %s.\n", args->arch);
        FAIL_AND_EXIT("Compilation failed for %s.\n", "Compilation failed for %s.", args->arch);
    }

    fprintf(thread_log_fp, "...Compilation successful for %s.\n", args->arch);
    APPEND_STAT_OR_FAIL("Compilation: done\n");
    completed_tasks++;

#ifdef TEST_ENABLED
    fprintf(thread_log_fp, "Running tests in chroot for %s...\n", args->arch);
    // Run tests (if enabled) inside the chroot
    char target_chroot_bin_path[MAX_CONFIG_ATTR_LEN*2];
    snprintf(target_chroot_bin_path, sizeof(target_chroot_bin_path), "%s/bin/sshlirp-%s", args->thread_chroot_target_dir, args->arch);
    if (test_sshlirp_bin(args, target_chroot_bin_path, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "...Tests failed for %s.\n", args->arch);
        APPEND_STAT_OR_FAIL("Tests: failed\n");
    }
    else {
        fprintf(thread_log_fp, "...Tests passed for %s.\n", args->arch);
        APPEND_STAT_OR_FAIL("Tests: passed\n");
        completed_tasks++;
    }
#endif

    fprintf(thread_log_fp, "Removing sources copy for %s.\n", args->arch);

    // Deleting source copies (chroot level operations, does not require mutex)
    if (remove_sources_copy_from_chroot(args, thread_log_fp) != 0) {
        FAIL_AND_EXIT("Error: Failed to remove sources copy for %s.\n", "Failed to remove sources copy for %s.", args->arch);
    }
    APPEND_STAT_OR_FAIL("Sources removal: done\n");
    completed_tasks++;

    fprintf(thread_log_fp, "Worker finished successfully for arch %s.\n", args->arch);
    
    result->status = 0;
    APPEND_PROGRESS_STAT();
    fclose(thread_log_fp);
    pthread_exit(result);
}