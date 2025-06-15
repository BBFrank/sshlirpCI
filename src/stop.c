#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "daemon_utils.h"

#define MAX_WAIT_SECONDS 600

int main() {
    FILE *pid_file_ptr;
    pid_t daemon_pid;

    // 1. Leggo il PID del demone
    pid_file_ptr = fopen(PID_FILE, "r");
    if (!pid_file_ptr) {
        fprintf(stderr, "Impossibile aprire il file PID %s (%s)\n", PID_FILE, strerror(errno));
        return 1;
    }

    if (fscanf(pid_file_ptr, "%d", &daemon_pid) != 1) {
        fprintf(stderr, "Impossibile leggere il PID dal file %s.\n", PID_FILE);
        fclose(pid_file_ptr);
        return 1;
    }
    fclose(pid_file_ptr);

    // 2. Controllo se il processo demone è attivo
    if (kill(daemon_pid, 0) != 0) {
        fprintf(stderr, "Il processo demone con PID %d non è in esecuzione.\n", daemon_pid);
        return 1;
    }

    printf("Tentativo di terminare il demone sshlirp_ci (PID: %d)...\n", daemon_pid);
    printf("Questo processo potrebbe dover attendere fino a %d secondi prima che il demone entri in stato di SLEEPING.\n", MAX_WAIT_SECONDS);

    FILE *state_file_ptr;

    // 3. Loop per attendere lo stato SLEEPING
    int attempts = 0;
    while (attempts < MAX_WAIT_SECONDS) {
        
        // Controllo che il demone non sia morto per cause naturali
        if (kill(daemon_pid, 0) != 0) {
            fprintf(stderr, "Il processo demone %d è morto da solo.\n", daemon_pid);
            remove(PID_FILE);
            remove(STATE_FILE);
            return 1;
        }

        char current_state[50] = {0};
        state_file_ptr = fopen(STATE_FILE, "r");
        if (!state_file_ptr && attempts == 0) {
            fprintf(stderr, "Avviso: Impossibile leggere il file di stato %s (%s). Possibile incoerenza o il demone sta entrando in fase di terminazione.\n", STATE_FILE, strerror(errno));
        }
        
        if (state_file_ptr) {
            if (fgets(current_state, sizeof(current_state), state_file_ptr) == NULL) {
                fprintf(stderr, "Errore nella lettura del file di stato %s: %s\n", STATE_FILE, strerror(errno));
                fclose(state_file_ptr);
                return 1;
            }
            current_state[strcspn(current_state, "\n")] = 0;
        }

        if (strcmp(current_state, DAEMON_STATE_SLEEPING) == 0) {
            printf("Il demone è in stato SLEEPING. Invio SIGTERM...\n");
            if (kill(daemon_pid, SIGTERM) == 0) {

                printf("Segnale SIGTERM inviato con successo.\n");
                // Attendo un po' che il demone termini e pulisca i suoi file
                sleep(3);
                // Controllo se i file sono stati rimossi dal demone
                if (access(PID_FILE, F_OK) == 0) {
                    printf("Il demone non ha pulito il file PID, lo rimuovo.\n");
                    remove(PID_FILE);
                }
                if (access(STATE_FILE, F_OK) == 0) {
                    printf("Il demone non ha pulito il file di stato, lo rimuovo.\n");
                    remove(STATE_FILE);
                }
                printf("Demone sshlirp_ci terminato.\n");
                fclose(state_file_ptr);
                return 0;

            } else {

                fprintf(stderr, "Errore durante l'invio di SIGTERM al PID %d: %s\n", daemon_pid, strerror(errno));
                fclose(state_file_ptr);
                return 1;
            }
        } else {

            if (attempts == 0) {
                printf("Il demone è attualmente in stato '%s'. Quindi attendo...\n", strlen(current_state) > 0 ? current_state : "UNKNOWN");
            }

            sleep(1);
            attempts++;
        }
        fclose(state_file_ptr);
    }

    fprintf(stderr, "Timeout: Il demone non è entrato in stato SLEEPING entro %d secondi.\n", MAX_WAIT_SECONDS);
    return 1;
}
