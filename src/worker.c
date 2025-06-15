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
    int* worker_status_ptr = malloc(sizeof(int));
    if (!worker_status_ptr) {
        printf("[Thread %s] Critical error: Failed to allocate memory for worker status.\n", args->arch);
        pthread_exit(NULL); 
    }
    *worker_status_ptr = 1;

    // Creo il file di log del thread nell'host se non esiste (nota: la directory che contiene i log file
    // di tutti i thread è stata creata dall'init del main)
    FILE* thread_log_fp = fopen(args->thread_log_file, "a");
    if (!thread_log_fp) {
        fprintf(stderr, "Failed to open thread log file %s: %s\n", args->thread_log_file, strerror(errno));
        pthread_exit(worker_status_ptr);
    }
    fclose(thread_log_fp);

    // Reindirizzo stdout e stderr al file di log personale del thred nell'host (quando entrerò nel chroot,
    // userò il file di log nel chroot)
    if (freopen(args->thread_log_file, "a", stdout) == NULL) {
        fprintf(stderr, "Failed to redirect stdout to thread log file, for arch %s\n", args->arch);
    }
    if (freopen(args->thread_log_file, "a", stderr) == NULL) {
        fprintf(stderr, "Failed to redirect stderr to thread log file, for arch %s\n", args->arch);
    }

    printf("Worker started for arch %s. Pull round: %d.", args->arch, args->pull_round);

    if (args->pull_round == 0) {
        printf("First run (pull_round 0). Checking and eventually setting up chroot for %s.", args->arch);
        // Non blocco questa operazione interamente in quanto l'unica operazione di scrittura nella stessa directory è la creazione del chroot, mentre il suo setup
        // (che prevede sole operazioni di lettura su risorse condivise e scrittura su risorse private del thread) può avvenire senza lock.
        // Quindi userò il lock solo per il mkdir del chroot
        if (setup_chroot(args) != 0) {
            printf("Failed to check/setup chroot for %s.", args->arch);
            pthread_exit(worker_status_ptr);
        }
        printf("Chroot setup complete for %s.", args->arch);

        // L'operazione di check/creazione delle directory del worker dentro il chroot può avvenire senza lock
        printf("Checking and eventually creating worker directories and log file inside chroot for %s.", args->arch);
        if (check_worker_dirs(args) != 0) {
            printf("Failed to check/create worker directories for %s.", args->arch);
            pthread_exit(worker_status_ptr);
        }
        printf("Worker directories and log file checked/created for %s.", args->arch);

    } else {
        printf("Not first run (pull_round %d). Skipping chroot setup and dir check for %s.", args->pull_round, args->arch);
    }

    printf("Copying sources into chroot for %s.", args->arch);

    // Non blocco questa operazione con il mutex in quanto andrò solo a copiare lo stesso codice sorgente di sshlirp/libslirp (operazione di lettura)
    if (copy_sources_to_chroot(args) != 0) {
        printf("Failed to copy sources for %s.", args->arch);
        pthread_exit(worker_status_ptr);
    }
    printf("Sources copied for %s.", args->arch);

    // Compilazione (avviene dentro il chroot quindi i log andranno in args->thread_chroot_log_file)
    printf("Starting compilation process in chroot for %s...", args->arch);
    if (compile_and_verify_in_chroot(args) != 0) {
        printf("...Compilation process failed for %s. Removing sources copy...", args->arch);

        // Eliminazione copie del sorgente anche in caso di fallimento della compilazione
        if (remove_sources_copy_from_chroot(args) != 0) {
            printf("Error: Failed to remove sources copy for %s.", args->arch);
            pthread_exit(worker_status_ptr);
        }
        pthread_exit(worker_status_ptr);
    }
    printf("...Compilation successful for %s.", args->arch);

    printf("Removing sources copy for %s.", args->arch);

    // Eliminazione copie del sorgente delle sorgenti (operazioni a livello chroot, non richiede mutex)
    if (remove_sources_copy_from_chroot(args) != 0) {
        printf("Error: Failed to remove sources copy for %s.", args->arch);
        pthread_exit(worker_status_ptr);
    }
    printf("Sources copy removed for %s.", args->arch);

    printf("Worker finished successfully for arch %s.", args->arch);

    *worker_status_ptr = 0;
    pthread_exit(worker_status_ptr);
}