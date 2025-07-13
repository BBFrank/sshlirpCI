#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include "types/types.h"
#include "init/init.h"
#include "worker.h"
#include "daemon_utils.h"
#include "utils/utils.h"

volatile sig_atomic_t terminate_daemon_flag = 0;

static void sigterm_handler(int signum) {
    if (signum == SIGTERM) {
        terminate_daemon_flag = 1;
    }
}

static void cleanup_daemon_files() {
    remove(PID_FILE);
    remove(STATE_FILE);
}

static void update_daemon_state(const char *state) {
    FILE *fp = fopen(STATE_FILE, "w");
    if (fp) {
        fprintf(fp, "%s", state);
        fclose(fp);
    } else {
        perror("Failed to update daemon state file");
    }
}

static void daemonize() {
    pid_t pid;

    // Fork off: the parent terminates and the child will be responsible for starting the daemon
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Create a new session for the daemon
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGTERM, sigterm_handler);

    // Fork again so I'll be a child of the session leader and I'm sure I won't have access to the terminal
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Try to change the working directory to /
    chdir("/");
}

static void log_time(FILE *log_file) {
    time_t now;
    struct tm *local_time;
    char time_buffer[80];

    now = time(NULL);
    local_time = localtime(&now);

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(log_file, "[%s] ", time_buffer);
}

int main() {
    // 0. Load variables from the configuration file
    char** archs_list = (char**)malloc(MAX_ARCHITECTURES * sizeof(char*));
    int num_archs = 0;
    char *sshlirp_repo_url = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *libslirp_repo_url = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *vdens_repo_url = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *main_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *versioning_file = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *target_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *sshlirp_source_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *libslirp_source_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *vdens_source_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *log_file = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *thread_chroot_target_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *thread_chroot_log_file = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    char *thread_log_dir = (char*)malloc(MAX_CONFIG_ATTR_LEN * sizeof(char));
    int poll_interval = 0;
    // Note: the mutex will only be necessary for the chroot setup, a very expensive operation that, if performed in parallel
    // for many architectures, risks race conditions
    pthread_mutex_t chroot_setup_mutex;
    pthread_mutex_init(&chroot_setup_mutex, NULL);

    printf("Starting sshlirp_ci...\n");
    printf("Loading configuration variables...\n");

    if(conf_vars_loader(
            archs_list, 
            &num_archs, 
            sshlirp_repo_url, 
            libslirp_repo_url, 
            vdens_repo_url,
            main_dir,
            versioning_file,
            target_dir, 
            sshlirp_source_dir,
            libslirp_source_dir,
            vdens_source_dir,
            log_file, 
            thread_chroot_target_dir,
            thread_chroot_log_file,
            thread_log_dir,
            &poll_interval) != 0) {
        fprintf(stderr, "Failed to load configuration variables. Exiting.\n");
        // (freeing previously allocated memory, in case of error, is handled by conf_vars_loader itself)
        return 1;
    }
    
    printf("Configuration loaded successfully.\n");
    printf("Checking for active daemon instances...\n");

    // 1. Check if another instance of the daemon is already running
    FILE* pid_fp_check = fopen(PID_FILE, "r");
    if (pid_fp_check) {
        pid_t old_pid;
        if (fscanf(pid_fp_check, "%d", &old_pid) == 1) {
            if (kill(old_pid, 0) == 0) {
                fprintf(stderr, "Error: sshlirp_ci is already running with PID %d.\n", old_pid);
                fclose(pid_fp_check);
                return 1;
            }
        }
        fclose(pid_fp_check);
    }

    printf("Daemonizing... Logs will be available in %s. To terminate the process, run: sudo ./sshlirp_ci_stop\n", log_file);
    
    // 2. Daemonize the process
    daemonize();

    // ============== Code executed only by the daemon from here on ==============

    // Write the daemon's PID
    FILE *pid_fp = fopen(PID_FILE, "w");
    if (pid_fp) {
        fprintf(pid_fp, "%d\n", getpid());
        fclose(pid_fp);
    } else {
        exit(EXIT_FAILURE); 
    }
    
    // Register the cleanup function on exit
    atexit(cleanup_daemon_files);

    // 3. Create main files (if they don't exist):
    // - the fundamental directory (/home/sshlirpCI)
    // - the main log directory and file (home/sshlirpCI/log and home/sshlirpCI/log/main_sshlirp.log)
    // - the versioning file (/home/sshlirpCI/versions.txt)

    // /home/sshlirpCI
    if (access(main_dir, F_OK) == -1) {
        if (mkdir(main_dir, 0755) == -1) {
            perror("Error creating main directory");
            return 1;
        }
    }
    // /home/sshlirpCI/versions.txt
    FILE* versioning_fp = fopen(versioning_file, "a");
    if (!versioning_fp) {
        perror("Error opening/creating versioning file");
        return 1;
    }
    fclose(versioning_fp);
    // /home/sshlirpCI/log
    char *log_dir = get_parent_dir(log_file);
    if (!log_dir) {
        perror("Error getting parent directory for log file");
        return 1;
    }
    if (access(log_dir, F_OK) == -1) {
        if (mkdir(log_dir, 0755) == -1) {
            perror("Error creating log directory");
            free(log_dir);
            return 1;
        }
    }
    free(log_dir);
    // /home/sshlirpCI/log/main_sshlirp.log
    FILE* log_fp = fopen(log_file, "a");
    if (!log_fp) {
        perror("Error opening log file");
        return 1;
    }
    // Set line buffering for the main log file, so that prints are written immediately after each newline
    setvbuf(log_fp, NULL, _IOLBF, 0);
    // I don't close log_fp here, I'll close it at the end of main, as I need it to write the daemon's logs

    int round = 0;
    commit_status_t initial_check = {1, NULL};
    commit_status_t new_commit = {1, NULL};

    // 5. Start the main loop in the daemon
    while (1) {
        if (terminate_daemon_flag) {
            fprintf(log_fp, "Termination signal received before starting operations, exiting...\n");
            break;
        }
        update_daemon_state(DAEMON_STATE_WORKING);

        // 6. Check if the host directories and git repositories exist
        if (round == 0) {
            log_time(log_fp);
            fprintf(log_fp, "Starting the daemon for the first time...\n");

            initial_check = check_host_dirs(target_dir, sshlirp_source_dir, libslirp_source_dir, vdens_source_dir, log_file, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, thread_log_dir, log_fp, versioning_file);

            // Note: this function does nothing if the dirs already exist and if the git repo already exists (possible in case of a crash or interruption)
            if (initial_check.status != 0 && initial_check.status != 2) {
                fprintf(log_fp, "Error: Error during search or creation of host directories, log file, or during repository cloning. Status: %d\n", initial_check.status);
                terminate_daemon_flag = 1;
                break;
            }
        }

        // 6.1. If it's not the first start (and so I had already cloned and waited poll_interval seconds) or if the repo was already cloned
        // (so maybe there was a crash or an interruption), I try to pull any new commits
        if (round > 0 || initial_check.status == 0) {
            new_commit = check_new_commit(sshlirp_source_dir, sshlirp_repo_url, libslirp_source_dir, libslirp_repo_url, log_file, log_fp, versioning_file);
        }

        // 7. If it's the first start and I actually cloned or if I found new commits, I prepare the threads for the build
        if ((round == 0 && initial_check.status == 2) || new_commit.status == 2) {

            if (round == 0) {
                fprintf(log_fp, "First daemon run, it's time to launch the threads...\n");
            } else {
                fprintf(log_fp, "\n");
                log_time(log_fp);
                fprintf(log_fp, "New commit for sshlirp found, proceeding with the build...\n");
            }

            // 7.1. Prepare the threads
            pthread_t threads[num_archs];
            thread_args_t args[num_archs];

            // 7.2. Launch the build threads
            for (int i = 0; i < num_archs; i++) {

                // Inizializzo il pull_round (mi sarà utile nel thread per capire se devo setuppare il chroot, il log file locale... o meno)
                args[i].pull_round = round;

                // Copia sicura del nome dell'architettura
                strncpy(args[i].arch, archs_list[i], sizeof(args[i].arch) - 1);
                args[i].arch[sizeof(args[i].arch) - 1] = '\0';

                // Copia sicura del percorso della directory di codice sorgente di sshlirp nell'host (mi servirà per copiare nel chroot)
                strncpy(args[i].sshlirp_host_source_dir, sshlirp_source_dir, sizeof(args[i].sshlirp_host_source_dir) - 1);
                args[i].sshlirp_host_source_dir[sizeof(args[i].sshlirp_host_source_dir) - 1] = '\0';

                // Copia sicura del percorso della directory di codice sorgente di libslirp nell'host (mi servirà per copiare nel chroot)
                strncpy(args[i].libslirp_host_source_dir, libslirp_source_dir, sizeof(args[i].libslirp_host_source_dir) - 1);
                args[i].libslirp_host_source_dir[sizeof(args[i].libslirp_host_source_dir) - 1] = '\0';

                // Copia sicura del percorso della directory di codice sorgente di vdens nell'host (se il testing è abilitato, mi servirà per copiare nel chroot)
                strncpy(args[i].vdens_host_source_dir, vdens_source_dir, sizeof(args[i].vdens_host_source_dir) - 1);
                args[i].vdens_host_source_dir[sizeof(args[i].vdens_host_source_dir) - 1] = '\0';

                // Copia sicura del chroot_path
                snprintf(args[i].chroot_path, sizeof(args[i].chroot_path), "%s/%s-chroot", main_dir, archs_list[i]);

                // Copia sicura della directory principale del thread (ossia dove, nel chroot, il thread dovrà lavorare -> come percorso "relativo" corrisponde alla main dir dell'host)
                strncpy(args[i].thread_chroot_main_dir, main_dir, sizeof(args[i].thread_chroot_main_dir) - 1);
                args[i].thread_chroot_main_dir[sizeof(args[i].thread_chroot_main_dir) - 1] = '\0';

                // Copia sicura della directory di codice sorgente di sshlirp nel chroot (il path relativo sarà lo stesso di quello usato nell'host)
                strncpy(args[i].thread_chroot_sshlirp_dir, sshlirp_source_dir, sizeof(args[i].thread_chroot_sshlirp_dir) - 1);
                args[i].thread_chroot_sshlirp_dir[sizeof(args[i].thread_chroot_sshlirp_dir) - 1] = '\0';

                // Copia sicura della directory di codice sorgente di libslirp nel chroot (idem)
                strncpy(args[i].thread_chroot_libslirp_dir, libslirp_source_dir, sizeof(args[i].thread_chroot_libslirp_dir) - 1);
                args[i].thread_chroot_libslirp_dir[sizeof(args[i].thread_chroot_libslirp_dir) - 1] = '\0';

                // Copia sicura della directory di codice sorgente di vdens nel chroot (idem, se il testing è abilitato)
                strncpy(args[i].thread_chroot_vdens_dir, vdens_source_dir, sizeof(args[i].thread_chroot_vdens_dir) - 1);
                args[i].thread_chroot_vdens_dir[sizeof(args[i].thread_chroot_vdens_dir) - 1] = '\0';

                // Copia sicura del thread_target_dir (ossia dove, nel chroot, il thread dovrà inserire il binario)
                strncpy(args[i].thread_chroot_target_dir, thread_chroot_target_dir, sizeof(args[i].thread_chroot_target_dir) - 1);
                args[i].thread_chroot_target_dir[sizeof(args[i].thread_chroot_target_dir) - 1] = '\0';

                // Copia sicura del thread_chroot_log_file (il log file "personale" del thread)
                strncpy(args[i].thread_chroot_log_file, thread_chroot_log_file, sizeof(args[i].thread_chroot_log_file) - 1);
                args[i].thread_chroot_log_file[sizeof(args[i].thread_chroot_log_file) - 1] = '\0';

                // Copia sicura del thread_log_file (ossia il log file su cui scriverà il thread quando non è nel chroot)
                snprintf(args[i].thread_log_file, sizeof(args[i].thread_log_file), "%s/%s-thread.log", thread_log_dir, archs_list[i]);

                // Assegnamento del mutex condiviso
                args[i].chroot_setup_mutex = &chroot_setup_mutex;

                if (pthread_create(&threads[i], NULL, build_worker, &args[i]) != 0) {
                    fprintf(log_fp, "Error: Error creating thread for architecture %s.\n", args[i].arch);
                    return 1;
                } else {
                    fprintf(log_fp, "Thread created successfully for architecture %s.\n", args[i].arch);
                }
            }

            // 7.3. Attendo che tutti i thread finiscano
            for (int i = 0; i < num_archs; i++) {
                void *thread_return_value;

                // Attendo il join del thread
                int successful_join = pthread_join(threads[i], &thread_return_value);

                if (successful_join != 0) {
                    fprintf(log_fp, "Error: Error joining thread for %s\n", args[i].arch);
                } else {
                    if (thread_return_value != NULL) {
                        thread_result_t *worker_result = (thread_result_t *)thread_return_value;
                        if (worker_result->status != 0) {
                            fprintf(log_fp, "Error: Thread for %s terminated with error: %s\n", args[i].arch, worker_result->error_message ? worker_result->error_message : "No error message.");
                        } else {
                            fprintf(log_fp, "Thread for %s terminated successfully.\n", args[i].arch);
                        }

                        // Free the memory allocated for the thread result
                        if (worker_result->error_message) {
                            free(worker_result->error_message);
                        }
                        free(worker_result);
                    } else {
                        fprintf(log_fp, "Thread for %s terminated without a specific return value (or error in return allocation).\n", args[i].arch);
                    }
                }
            }

            // 7.4. Merge the thread logs (logs on the host for each thread + logs in the chroot) into the main log and clean the thread logs (both in thread_log_dir and in thread_chroot_log_dir)
            for (int i = 0; i < num_archs; i++) {
                char thread_log_path_on_host[MAX_CONFIG_ATTR_LEN];
                snprintf(thread_log_path_on_host, sizeof(thread_log_path_on_host), "%s", args[i].thread_log_file);

                char thread_log_path_in_chroot[MAX_CONFIG_ATTR_LEN*2];
                snprintf(thread_log_path_in_chroot, sizeof(thread_log_path_in_chroot), "%s%s", args[i].chroot_path, args[i].thread_chroot_log_file);

                FILE *thread_log_read_on_host = fopen(thread_log_path_on_host, "r");
                FILE *thread_log_read_in_chroot = fopen(thread_log_path_in_chroot, "r");

                if (thread_log_read_on_host) {
                    char line[1024];

                    fprintf(log_fp, "===== Start Log from thread %s =====\n", args[i].arch);
                    fprintf(log_fp, "--- Start Log from thread %s (Host logfile - chroot setup, copy and remove sources logs: %s) ---\n", args[i].arch, thread_log_path_on_host);
                    while (fgets(line, sizeof(line), thread_log_read_on_host)) {
                        fprintf(log_fp, "%s", line);
                    }
                    fclose(thread_log_read_on_host);
                    fprintf(log_fp, "--- End Log from thread %s (Host logfile - chroot setup, copy and remove sources logs: %s) ---\n", args[i].arch, thread_log_path_on_host);

                    // Note: if the log file exists on the host, it doesn't necessarily mean it also exists inside the chroot (an error might have occurred)
                    if (thread_log_read_in_chroot) {
                        fprintf(log_fp, "--- Start Log from thread %s (Chroot logfile - compile and testing inside chroot logs: %s) ---\n", args[i].arch, thread_log_path_in_chroot);
                        while (fgets(line, sizeof(line), thread_log_read_in_chroot)) {
                            fprintf(log_fp, "%s", line);
                        }
                        fclose(thread_log_read_in_chroot);
                        fprintf(log_fp, "--- End Log from thread %s (Chroot logfile - compile and testing inside chroot logs: %s) ---\n", args[i].arch, thread_log_path_in_chroot);
                    } else {
                        fprintf(log_fp, "Warning: Could not open thread log for architecture %s in chroot: %s\n", args[i].arch, strerror(errno));
                    }

                    fprintf(log_fp, "===== End Log from thread %s =====\n", args[i].arch);

                    // Clean the thread log files by truncating them
                    FILE *thread_log_truncate = fopen(thread_log_path_on_host, "w");
                    if (thread_log_truncate) {
                        fclose(thread_log_truncate);
                        fprintf(log_fp, "Thread log %s (Host) cleaned successfully: %s\n", args[i].arch, thread_log_path_on_host);
                    } else {
                        fprintf(log_fp, "Error: Error cleaning (truncating) thread log for architecture %s: %s. Error: %s\n", args[i].arch, thread_log_path_on_host, strerror(errno));
                    }

                    if (access(thread_log_path_in_chroot, F_OK) == 0) {
                        FILE *thread_chroot_log_truncate = fopen(thread_log_path_in_chroot, "w");
                        if (thread_chroot_log_truncate) {
                            fclose(thread_chroot_log_truncate);
                            fprintf(log_fp, "Thread log %s (Chroot) cleaned successfully: %s\n", args[i].arch, thread_log_path_in_chroot);
                        } else {
                            fprintf(log_fp, "Error: Error cleaning (truncating) thread log for architecture %s (Chroot): %s. Error: %s\n", args[i].arch, thread_log_path_in_chroot, strerror(errno));
                        }
                    }

                } else {
                    fprintf(log_fp, "Warning: Could not open for reading the log file (Host) of the thread for architecture %s. Error: %s\n", args[i].arch, strerror(errno));
                }
            }

            // 7.5. Move the compiled binaries to target_dir/initial_check.new_release (or to target_dir/new_commit.new_release)
            for (int i = 0; i < num_archs; i++) {

                char expected_binary_name[MAX_CONFIG_ATTR_LEN];
                char source_bin_path[MAX_CONFIG_ATTR_LEN * 3 + 10];

                snprintf(expected_binary_name, sizeof(expected_binary_name), "sshlirp-%s", args[i].arch);
                snprintf(source_bin_path, sizeof(source_bin_path), "%s%s/bin/%s", args[i].chroot_path, args[i].thread_chroot_target_dir, expected_binary_name);

                char final_target_dir[MAX_CONFIG_ATTR_LEN*2];
                char final_target_path[MAX_CONFIG_ATTR_LEN*3];

                if (new_commit.status == 2) {
                    snprintf(final_target_dir, sizeof(final_target_dir), "%s/%s", target_dir, new_commit.new_release);
                } else {
                    snprintf(final_target_dir, sizeof(final_target_dir), "%s/%s", target_dir, initial_check.new_release);
                }

                if (access(final_target_dir, F_OK) == -1) {
                    if (mkdir(final_target_dir, 0755) != 0) {
                        fprintf(log_fp, "Error: Error creating directory %s for architecture %s. Error: %s. Binaries for this architecture will be placed in the parent directory of the release.\n", final_target_dir, args[i].arch, strerror(errno));
                        snprintf(final_target_path, sizeof(final_target_path), "%s/%s", target_dir, expected_binary_name);
                    } else {
                        fprintf(log_fp, "Directory %s created successfully for the new release (architecture %s).\n", final_target_dir, args[i].arch);
                        snprintf(final_target_path, sizeof(final_target_path), "%s/%s", final_target_dir, expected_binary_name);
                    }
                } else {
                    fprintf(log_fp, "Directory %s already exists for the release (architecture %s).\n", final_target_dir, args[i].arch);
                    snprintf(final_target_path, sizeof(final_target_path), "%s/%s", final_target_dir, expected_binary_name);
                }

                if (access(source_bin_path, F_OK) == 0) {
                    if (access(final_target_path, F_OK) == 0) {
                        if (remove(final_target_path) != 0) {
                            fprintf(log_fp, "Error: Error removing old binary %s for architecture %s. Error: %s\n", final_target_path, args[i].arch, strerror(errno));
                        } else {
                            fprintf(log_fp, "Old binary for architecture %s removed successfully.\n", args[i].arch);
                        }
                    }

                    if (rename(source_bin_path, final_target_path) != 0) {
                        fprintf(log_fp, "Error: Error moving binary %s to %s for architecture %s. Error: %s\n", source_bin_path, final_target_path, args[i].arch, strerror(errno));
                    } else {
                        fprintf(log_fp, "Binary for architecture %s moved successfully to %s.\n", args[i].arch, final_target_path);
                    }
                } else {
                    fprintf(log_fp, "Error: Source binary %s not found for architecture %s. Move skipped.\n", source_bin_path, args[i].arch);
                }
            }

            fprintf(log_fp, "\n");
            log_time(log_fp);
            fprintf(log_fp, "Build completed for all architectures.\n");

        }/*  else if (round == 0 && initial_check.status == 1 && new_commit.status == 1) {
            // impossible: initial_check.status == 1 would mean I had an error during check_host_dirs,
            // so I can't be here.
            // Note: this scenario might seem to be associated with an "empty" execution (I got here with still the initialization values).
            // But this is not possible because with round == 0 I execute check_host_dirs, which leaves 
            // initial_check.status == 1 only if it fails, but in that case it would exit the loop and could never get here.

        } else if (round == 0 && initial_check.status == 0 && new_commit.status == 1) {
            // On the first run I found everything already cloned and I tried to pull but there was an error.
            // In this case I have to break (an error in the pull is critical, I can't keep the daemon running)
            fprintf(log_fp, "Error during check_new_commit on first run: repos already cloned were found but the pull failed. Exiting daemon...\n");
            break;

        } else if (round > 0 && initial_check.status == 1 && new_commit.status == 1) {
            // impossible: initial_check.status == 1 would mean I had an error during check_host_dirs,
            // so I can't be here. Moreover, round > 0 in this context is paradoxical

        } else if (round > 0 && initial_check.status == 0 && new_commit.status == 1) {
            // On the first run I had registered that the dirs on the host were already present and the host was already correctly configured (there had been an interruption),
            // so I went on with the rounds (during which the check_new_commit were also performed correctly).
            // At this point, however, an error was encountered during the pull of this round and so I have to exit the daemon
            fprintf(log_fp, "Error during check_new_commit: on startup repos were already cloned, %d rounds were performed correctly but on round %d the pull failed. Exiting daemon...\n", round - 1, round);
            break;

        } else if (round > 0 && initial_check.status == 2 && new_commit.status == 1) {
            // On the first run I had correctly cloned the repos, then I went on with the rounds (during which the check_new_commit were also performed correctly),
            // but at this point I encountered an error during check_new_commit; I must therefore exit the daemon
            fprintf(log_fp, "Error during check_new_commit: on first run the repos were cloned correctly, %d rounds were performed correctly but on round %d the pull failed. Exiting daemon...\n", round - 1, round);
            break;

        } else if (round == 0 && initial_check.status == 1 && new_commit.status == 0) {
            // impossible: initial_check.status == 1 would mean I had an error during check_host_dirs,
            // so I can't be here. Also because new_commit_status == 0 would mean I executed
            // check_new_commit despite the check_host_dirs error

        } else if (round == 0 && initial_check.status == 0 && new_commit.status == 0) {
            // On the first run everything was found in its place on the host (the dirs and repos were already present),
            // check_new_commit was then performed but no new commits were found.
            // This is one of the cases where I simply have to move on

        } else if (round > 0 && initial_check.status == 1 && new_commit.status == 0) {
            // impossible: initial_check.status == 1 would mean I had an error during check_host_dirs,
            // so I can't be here. Moreover, round > 0 and new_commit_status == 0 in this context are paradoxical

        } else if (round > 0 && initial_check.status == 0 && new_commit.status == 0) {
            // On the first run everything was found in its place on the host (the dirs and repos were already present),
            // several rounds were then performed and the last check_new_commit found no new commits...
            // Here too: all normal, I move on

        } else if (round > 0 && initial_check.status == 2 && new_commit.status == 0) {
            // On the first run I had correctly cloned the repos, then I went on with the rounds (during which the check_new_commit were also performed correctly),
            // and at this point I did check_new_commit again but no new commits were found.
            // In this case too, I just have to move on

        } */

        // According to the previous considerations, it is possible to summarize the possible outcomes of the checks in the following cases:
        else if (new_commit.status == 1) {
            fprintf(log_fp, "Error: Error during check_new_commit() or check_host_dirs() call. Exiting daemon...\n");
            break;
        }
        else {                                                  // new_commit.status == 0
            // no new commit found, moving on
        }
        
        if (terminate_daemon_flag) {
            fprintf(log_fp, "Termination signal received before sleep and after operations completed, exiting...\n");
            break;
        }

        update_daemon_state(DAEMON_STATE_SLEEPING);
        log_time(log_fp);
        fprintf(log_fp, "Daemon sleeping for %d seconds...\n", poll_interval);
        
        // Sleep cycle and handling of interrupt signals
        unsigned int time_left = poll_interval;
        while(time_left > 0) {
            time_left = sleep(time_left);
            if (terminate_daemon_flag) {
                fprintf(log_fp, "Sleep interrupted by termination signal.\n");
                break; 
            }
            if (time_left > 0) {
                // If I had time left to sleep and I didn't receive a stop signal, then I was disturbed by someone else and so I ignore and sleep again
                fprintf(log_fp, "Sleep interrupted, %u seconds remaining, continuing to wait...\n", time_left);
            }
        }

        round++;
    }

    log_time(log_fp);
    fprintf(log_fp, "sshlirp_ci daemon terminated.\n");
    fclose(log_fp);

    pthread_mutex_destroy(&chroot_setup_mutex);

    // Free allocated memory
    for (int i = 0; i < num_archs; i++) {
        free(archs_list[i]);
    }
    free(archs_list);
    free(sshlirp_repo_url);
    free(libslirp_repo_url);
    free(vdens_repo_url);
    free(versioning_file);
    free(main_dir);
    free(target_dir);
    free(sshlirp_source_dir);
    free(libslirp_source_dir);
    free(log_file);
    free(thread_chroot_target_dir);
    free(thread_chroot_log_file);
    free(thread_log_dir);

    return 0;
}