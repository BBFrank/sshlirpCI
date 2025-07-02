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

    // 1. Read the daemon's PID
    pid_file_ptr = fopen(PID_FILE, "r");
    if (!pid_file_ptr) {
        fprintf(stderr, "Could not open PID file %s (%s)\n", PID_FILE, strerror(errno));
        return 1;
    }

    if (fscanf(pid_file_ptr, "%d", &daemon_pid) != 1) {
        fprintf(stderr, "Could not read PID from file %s.\n", PID_FILE);
        fclose(pid_file_ptr);
        return 1;
    }
    fclose(pid_file_ptr);

    // 2. Check if the daemon process is active
    if (kill(daemon_pid, 0) != 0) {
        fprintf(stderr, "Daemon process with PID %d is not running.\n", daemon_pid);
        return 1;
    }

    printf("Attempting to terminate sshlirp_ci daemon (PID: %d)...\n", daemon_pid);
    printf("This process may need to wait up to %d seconds for the daemon to enter SLEEPING state.\n", MAX_WAIT_SECONDS);

    FILE *state_file_ptr;

    // 3. Loop to wait for SLEEPING state
    int attempts = 0;
    while (attempts < MAX_WAIT_SECONDS) {
        
        // Check if the daemon died of natural causes
        if (kill(daemon_pid, 0) != 0) {
            fprintf(stderr, "Daemon process %d died on its own.\n", daemon_pid);
            remove(PID_FILE);
            remove(STATE_FILE);
            return 1;
        }

        char current_state[50] = {0};
        state_file_ptr = fopen(STATE_FILE, "r");
        if (!state_file_ptr && attempts == 0) {
            fprintf(stderr, "Warning: Could not read state file %s (%s). Possible inconsistency or the daemon is entering termination phase.\n", STATE_FILE, strerror(errno));
        }
        
        if (state_file_ptr) {
            if (fgets(current_state, sizeof(current_state), state_file_ptr) == NULL) {
                fprintf(stderr, "Error reading state file %s: %s\n", STATE_FILE, strerror(errno));
                fclose(state_file_ptr);
                return 1;
            }
            current_state[strcspn(current_state, "\n")] = 0;
        }

        if (strcmp(current_state, DAEMON_STATE_SLEEPING) == 0) {
            printf("Daemon is in SLEEPING state. Sending SIGTERM...\n");
            if (kill(daemon_pid, SIGTERM) == 0) {

                printf("SIGTERM signal sent successfully.\n");
                // Wait a bit for the daemon to terminate and clean up its files
                sleep(3);
                // Check if the files were removed by the daemon
                if (access(PID_FILE, F_OK) == 0) {
                    printf("Daemon did not clean up PID file, removing it.\n");
                    remove(PID_FILE);
                }
                if (access(STATE_FILE, F_OK) == 0) {
                    printf("Daemon did not clean up state file, removing it.\n");
                    remove(STATE_FILE);
                }
                printf("sshlirp_ci daemon terminated.\n");
                fclose(state_file_ptr);
                return 0;

            } else {

                fprintf(stderr, "Error sending SIGTERM to PID %d: %s\n", daemon_pid, strerror(errno));
                fclose(state_file_ptr);
                return 1;
            }
        } else {

            if (attempts == 0) {
                printf("Daemon is currently in '%s' state. Waiting...\n", strlen(current_state) > 0 ? current_state : "UNKNOWN");
            }

            sleep(1);
            attempts++;
        }
        fclose(state_file_ptr);
    }

    fprintf(stderr, "Timeout: Daemon did not enter SLEEPING state within %d seconds.\n", MAX_WAIT_SECONDS);
    return 1;
}
