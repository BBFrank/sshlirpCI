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

void *build_worker(void *arg_ptr) {
    thread_args_t* args = (thread_args_t*)arg_ptr;
    thread_result_t* result = malloc(sizeof(thread_result_t));
    if (!result) {
        pthread_exit(NULL); 
    }
    result->status = 1;
    result->error_message = NULL;

    // Creo il file di log del thread nell'host se non esiste (nota: la directory che contiene i log file
    // di tutti i thread è stata creata dall'init del main)
    FILE* thread_log_fp = fopen(args->thread_log_file, "a");
    if (!thread_log_fp) {
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Failed to open thread log file %s: %s", args->thread_log_file, strerror(errno));
        result->error_message = strdup(err_buf);
        pthread_exit(result);
    }

    // Imposto il line buffering per il file di log del thread, in modo che le stampe vengano scritte subito dopo ogni newline
    setvbuf(thread_log_fp, NULL, _IOLBF, 0);

    fprintf(thread_log_fp, "Worker started for arch %s. Pull round: %d.\n", args->arch, args->pull_round);

    if (args->pull_round == 0) {
        fprintf(thread_log_fp, "First run (pull_round 0). Checking and eventually setting up chroot for %s.\n", args->arch);
        
        if (setup_chroot(args, thread_log_fp) != 0) {
            fprintf(thread_log_fp, "Failed to check/setup chroot for %s.\n", args->arch);
            char err_buf[MAX_CONFIG_ATTR_LEN*2];
            snprintf(err_buf, sizeof(err_buf), "Chroot check/setup failed for %s.", args->arch);
            result->error_message = strdup(err_buf);
            fclose(thread_log_fp);
            pthread_exit(result);
        }
        fprintf(thread_log_fp, "Chroot setup complete for %s.\n", args->arch);

        // L'operazione di check/creazione delle directory del worker dentro il chroot può avvenire senza lock
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

    // Non blocco questa operazione con il mutex in quanto andrò solo a copiare lo stesso codice sorgente di sshlirp/libslirp (operazione di lettura)
    if (copy_sources_to_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "Failed to copy sources for %s.\n", args->arch);
        char err_buf[MAX_CONFIG_ATTR_LEN*2];
        snprintf(err_buf, sizeof(err_buf), "Sources copy failed for %s.", args->arch);
        result->error_message = strdup(err_buf);
        fclose(thread_log_fp);
        pthread_exit(result);
    }
    fprintf(thread_log_fp, "Sources copied for %s.\n", args->arch);

    // Compilazione (avviene dentro il chroot quindi i log andranno in args->thread_chroot_log_file)
    fprintf(thread_log_fp, "Starting compilation process in chroot for %s...\n", args->arch);
    if (compile_and_verify_in_chroot(args, thread_log_fp) != 0) {
        fprintf(thread_log_fp, "...Compilation process failed for %s. Removing sources copy...\n", args->arch);

        // Eliminazione copie del sorgente anche in caso di fallimento della compilazione
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

    fprintf(thread_log_fp, "Removing sources copy for %s.\n", args->arch);

    // Eliminazione copie del sorgente delle sorgenti (operazioni a livello chroot, non richiede mutex)
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