#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "daemon_utils.h"

#define TERM_WAIT_SECONDS 10
#define CHECK_INTERVAL_MS 200
#define KILL_WAIT_SECONDS 2

static int process_alive(pid_t pid) {
    return kill(pid, 0) == 0;
}

int main(void) {
    FILE *pid_fp = fopen(PID_FILE, "r");
    if (!pid_fp) {
        fprintf(stderr, "PID file %s non trovato (%s). Pulizia residui...\n", PID_FILE, strerror(errno));
        remove(STATE_FILE);
        return 1;
    }

    pid_t daemon_pid;
    if (fscanf(pid_fp, "%d", &daemon_pid) != 1) {
        fprintf(stderr, "Impossibile leggere il PID da %s.\n", PID_FILE);
        fclose(pid_fp);
        return 1;
    }
    fclose(pid_fp);

    if (!process_alive(daemon_pid)) {
        fprintf(stderr, "Il processo %d non è in esecuzione. Pulizia file...\n", daemon_pid);
        remove(PID_FILE);
        remove(STATE_FILE);
        return 1;
    }

    char state_buf[64] = {0};
    FILE *state_fp = fopen(STATE_FILE, "r");
    if (state_fp) {
        if (fgets(state_buf, sizeof(state_buf), state_fp)) {
            state_buf[strcspn(state_buf, "\n")] = 0;
        }
        fclose(state_fp);
    }

    printf("Killer: terminazione immediata del daemon sshlirp_ci (PID %d). Stato attuale: %s\n",
           daemon_pid, state_buf[0] ? state_buf : "SCONOSCIUTO");

    // Primo tentativo: SIGTERM (terminazione “sicura” -> permette al codice di chiudere risorse se intercetta il segnale)
    if (kill(daemon_pid, SIGTERM) != 0) {
        fprintf(stderr, "Errore nell'invio di SIGTERM a %d: %s\n", daemon_pid, strerror(errno));
    } else {
        printf("SIGTERM inviato. Attendo fino a %d secondi...\n", TERM_WAIT_SECONDS);
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = CHECK_INTERVAL_MS * 1000000L;

        int waited_ms = 0;
        int max_wait_ms = TERM_WAIT_SECONDS * 1000;
        while (waited_ms < max_wait_ms) {
            if (!process_alive(daemon_pid)) {
                printf("Daemon terminato dopo SIGTERM (%d ms).\n", waited_ms);
                goto cleanup;
            }
            nanosleep(&ts, NULL);
            waited_ms += CHECK_INTERVAL_MS;
        }
        printf("Il daemon non è terminato entro %d secondi dopo SIGTERM.\n", TERM_WAIT_SECONDS);
    }

    // Escalation: SIGKILL
    if (process_alive(daemon_pid)) {
        printf("Invio SIGKILL al PID %d...\n", daemon_pid);
        if (kill(daemon_pid, SIGKILL) != 0) {
            fprintf(stderr, "Errore nell'invio di SIGKILL a %d: %s\n", daemon_pid, strerror(errno));
        } else {
            printf("SIGKILL inviato. Attendo fino a %d secondi...\n", KILL_WAIT_SECONDS);
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = CHECK_INTERVAL_MS * 1000000L;

            // Ripeto l'attesa ma con KILL_WAIT_SECONDS
            int waited_ms = 0;
            int kill_wait_ms = KILL_WAIT_SECONDS * 1000;
            while (waited_ms < kill_wait_ms) {
                if (!process_alive(daemon_pid)) {
                    printf("Daemon terminato forzatamente.\n");
                    goto cleanup;
                }
                nanosleep(&ts, NULL);
                waited_ms += CHECK_INTERVAL_MS;
            }
        }
    }

    if (process_alive(daemon_pid)) {
        fprintf(stderr, "Attenzione: il processo %d sembra ancora vivo dopo SIGKILL.\n", daemon_pid);
    }

cleanup:
    // Pulizia file (il daemon potrebbe non aver eseguito atexit)
    if (access(PID_FILE, F_OK) == 0) {
        if (remove(PID_FILE) == 0)
            printf("PID file rimosso.\n");
        else
            fprintf(stderr, "Impossibile rimuovere %s: %s\n", PID_FILE, strerror(errno));
    }
    if (access(STATE_FILE, F_OK) == 0) {
        if (remove(STATE_FILE) == 0)
            printf("STATE file rimosso.\n");
        else
            fprintf(stderr, "Impossibile rimuovere %s: %s\n", STATE_FILE, strerror(errno));
    }

    printf("Operazione killer completata con successo.\n");
    return 0;
}
