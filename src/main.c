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

    // Mi forko: il padre termina e il figlio avrà il compito di avviare il demone
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Creo una nuova sessione per il demone
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGTERM, sigterm_handler);

    // Mi forko nuovamente perchè così sarò figlio del session leader e sarò sicuro di non aver accesso al terminale
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Tento di cambiare la directory di lavoro a /
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
    // 0. Carico le variabili dal file di configurazione
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
    // Nota: il mutex sarà necessario solo per il setup del chroot, operazione molto dispendiosa che, se eseguita in parallelo
    // per molte architetture, rischia l'accadimento di race conditions
    pthread_mutex_t chroot_setup_mutex;
    pthread_mutex_init(&chroot_setup_mutex, NULL);

    printf("Avvio di sshlirp_ci...\n");
    printf("Caricamento delle variabili di configurazione...\n");

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
        // (il free della memoria allocata precedentemente, in caso di errore, è gestito dallo stesso conf_vars_loader)
        return 1;
    }
    
    printf("Configurazione caricata con successo.\n");
    printf("Verifica esistenza istanze del demone attive...\n");

    // 1. Controllo se un'altra istanza del demone è già in esecuzione
    FILE* pid_fp_check = fopen(PID_FILE, "r");
    if (pid_fp_check) {
        pid_t old_pid;
        if (fscanf(pid_fp_check, "%d", &old_pid) == 1) {
            if (kill(old_pid, 0) == 0) {
                fprintf(stderr, "Error: sshlirp_ci è già in esecuzione con PID %d.\n", old_pid);
                fclose(pid_fp_check);
                return 1;
            }
        }
        fclose(pid_fp_check);
    }

    printf("Daemonizzazione in corso... I log saranno reperibili in %s. Per terminare il processo eseguire: sudo ./sshlirp_ci_stop\n", log_file);
    
    // 2. Demonizzo il processo
    daemonize();

    // ============== Codice eseguito solo dal demone da qui in poi ==============

    // Scrivi il PID del demone
    FILE *pid_fp = fopen(PID_FILE, "w");
    if (pid_fp) {
        fprintf(pid_fp, "%d\n", getpid());
        fclose(pid_fp);
    } else {
        exit(EXIT_FAILURE); 
    }
    
    // Registra la funzione di cleanup all'uscita
    atexit(cleanup_daemon_files);

    // 3. Creo file principali (se non esistono):
    // - la directory fondamentale (/home/sshlirpCI)
    // - la directory e il file di log principale (home/sshlirpCI/log e home/sshlirpCI/log/main_sshlirp.log)
    // - il file di versioning (/home/sshlirpCI/versions.txt)

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
    // Imposto il line buffering per il file di log principale, in modo che le stampe vengano scritte subito dopo ogni newline
    setvbuf(log_fp, NULL, _IOLBF, 0);
    // non chiudo log_fp qui, lo chiuderò alla fine del main, in quanto mi serve per scrivere i log del demone

    int round = 0;
    commit_status_t initial_check = {1, NULL};
    commit_status_t new_commit = {1, NULL};

    // 5. Avvio il loop principale nel demone
    while (1) {
        if (terminate_daemon_flag) {
            fprintf(log_fp, "Segnale di terminazione ricevuto prima dell'avvio operazioni, uscita in corso...\n");
            break;
        }
        update_daemon_state(DAEMON_STATE_WORKING);

        // 6. Controllo se ci sono le directory dell'host e i repository git
        if (round == 0) {
            log_time(log_fp);
            fprintf(log_fp, "Avvio del demone per la prima volta...\n");

            initial_check = check_host_dirs(target_dir, sshlirp_source_dir, libslirp_source_dir, vdens_source_dir, log_file, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, thread_log_dir, log_fp, versioning_file);

            // Nota: questa funzione non fa nulla se le dirs sono già esistenti e se esiste già la repo git (possibile in caso di crash o interruzione)
            if (initial_check.status == 1) {
                fprintf(log_fp, "Error: Errore durante la ricerca o creazione delle directories dell'host, del file di log o durante la clonazione dei repository.\n");
                terminate_daemon_flag = 1;
                break;
            }
        }

        // 6.1. Se non è il primo avvio (e quindi avevo già clonato e ho atteso poll_interval secondi) o se la repo era già clonata
        // (quindi magari c'è stato un crash o un'interruzione), tento di pullare eventuali nuovi commit
        if (round > 0 || initial_check.status == 0) {
            new_commit = check_new_commit(sshlirp_source_dir, sshlirp_repo_url, libslirp_source_dir, libslirp_repo_url, log_file, log_fp, versioning_file);
        }

        // 7. Se è il primo avvio e ho effettivamente clonato o se ho trovato nuovi commit, preparo i thread per la build
        if ((round == 0 && initial_check.status == 2) || new_commit.status == 2) {

            if (round == 0) {
                fprintf(log_fp, "Primo avvio del demone, è il momento di lanciare i thread...\n");
            } else {
                fprintf(log_fp, "\n");
                log_time(log_fp);
                fprintf(log_fp, "Nuovo commit per sshlirp trovato, procedo con la build...\n");
            }

            // 7.1. Preparo i thread
            pthread_t threads[num_archs];
            thread_args_t args[num_archs];

            // 7.2. Lancio i thread di build
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
                    fprintf(log_fp, "Error: Errore durante la creazione del thread per l'architettura %s.\n", args[i].arch);
                    return 1;
                } else {
                    fprintf(log_fp, "Thread creato con successo per l'architettura %s.\n", args[i].arch);
                }
            }

            // 7.3. Attendo che tutti i thread finiscano
            for (int i = 0; i < num_archs; i++) {
                void *thread_return_value;

                // Attendo il join del thread
                int successful_join = pthread_join(threads[i], &thread_return_value);

                if (successful_join != 0) {
                    fprintf(log_fp, "Error: Errore durante il join del thread per %s\n", args[i].arch);
                } else {
                    if (thread_return_value != NULL) {
                        thread_result_t *worker_result = (thread_result_t *)thread_return_value;
                        if (worker_result->status != 0) {
                            fprintf(log_fp, "Error: Thread per %s terminato con errore: %s\n", args[i].arch, worker_result->error_message ? worker_result->error_message : "Nessun messaggio di errore.");
                        } else {
                            fprintf(log_fp, "Thread per %s terminato con successo.\n", args[i].arch);
                        }

                        // Libero la memoria allocata per il risultato del thread
                        if (worker_result->error_message) {
                            free(worker_result->error_message);
                        }
                        free(worker_result);
                    } else {
                        fprintf(log_fp, "Thread per %s terminato senza un valore di ritorno specifico (o errore nell'allocazione del ritorno).\n", args[i].arch);
                    }
                }
            }

            // 7.4. Faccio il merge dei log dei thread (logs nell'host di ogni thread + logs nel chroot) nel log principale e pulisco i log (sia quelli in thread_log_dir che in thread_chroot_log_dir) dei thread
            for (int i = 0; i < num_archs; i++) {
                char thread_log_path_on_host[MAX_CONFIG_ATTR_LEN];
                snprintf(thread_log_path_on_host, sizeof(thread_log_path_on_host), "%s", args[i].thread_log_file);

                char thread_log_path_in_chroot[MAX_CONFIG_ATTR_LEN*2];
                snprintf(thread_log_path_in_chroot, sizeof(thread_log_path_in_chroot), "%s%s", args[i].chroot_path, args[i].thread_chroot_log_file);

                FILE *thread_log_read_on_host = fopen(thread_log_path_on_host, "r");
                FILE *thread_log_read_in_chroot = fopen(thread_log_path_in_chroot, "r");

                if (thread_log_read_on_host) {
                    char line[1024];

                    fprintf(log_fp, "===== Inizio Log dal thread %s =====\n", args[i].arch);
                    fprintf(log_fp, "--- Inizio Log dal thread %s (Host logfile: %s) ---\n", args[i].arch, thread_log_path_on_host);
                    while (fgets(line, sizeof(line), thread_log_read_on_host)) {
                        fprintf(log_fp, "%s", line);
                    }
                    fclose(thread_log_read_on_host);
                    fprintf(log_fp, "--- Fine Log dal thread %s (Host logfile: %s) ---\n", args[i].arch, thread_log_path_on_host);

                    // Nota: se c'è il log file nell'host, non è detto che ci sia anche quello dentro il chroot (potrebbe esserci stato un errore)
                    if (thread_log_read_in_chroot) {
                        fprintf(log_fp, "--- Inizio Log dal thread %s (Chroot logfile: %s) ---\n", args[i].arch, thread_log_path_in_chroot);
                        while (fgets(line, sizeof(line), thread_log_read_in_chroot)) {
                            fprintf(log_fp, "%s", line);
                        }
                        fclose(thread_log_read_in_chroot);
                        fprintf(log_fp, "--- Fine Log dal thread %s (Chroot logfile: %s) ---\n", args[i].arch, thread_log_path_in_chroot);
                    } else {
                        fprintf(log_fp, "Avviso: Impossibile aprire il log del thread per l'architettura %s nel chroot: %s\n", args[i].arch, strerror(errno));
                    }

                    fprintf(log_fp, "===== Fine Log dal thread %s =====\n", args[i].arch);

                    // Pulisco i file di log del thread troncandoli
                    FILE *thread_log_truncate = fopen(thread_log_path_on_host, "w");
                    if (thread_log_truncate) {
                        fclose(thread_log_truncate);
                        fprintf(log_fp, "Log del thread %s (Host) pulito con successo: %s\n", args[i].arch, thread_log_path_on_host);
                    } else {
                        fprintf(log_fp, "Error: Errore nella pulizia (truncating) del log del thread per l'architettura %s: %s. Errore: %s\n", args[i].arch, thread_log_path_on_host, strerror(errno));
                    }

                    if (access(thread_log_path_in_chroot, F_OK) == 0) {
                        FILE *thread_chroot_log_truncate = fopen(thread_log_path_in_chroot, "w");
                        if (thread_chroot_log_truncate) {
                            fclose(thread_chroot_log_truncate);
                            fprintf(log_fp, "Log del thread %s (Chroot) pulito con successo: %s\n", args[i].arch, thread_log_path_in_chroot);
                        } else {
                            fprintf(log_fp, "Error: Errore nella pulizia (truncating) del log del thread per l'architettura %s (Chroot): %s. Errore: %s\n", args[i].arch, thread_log_path_in_chroot, strerror(errno));
                        }
                    }

                } else {
                    fprintf(log_fp, "Avviso: Impossibile aprire per la lettura il file di log (Host) del thread per l'architettura %s. Errore: %s\n", args[i].arch, strerror(errno));
                }
            }

            // 7.5. Sposto i binari compilati in target_dir/initial_check.new_release (oppure in target_dir/new_commit.new_release)
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
                        fprintf(log_fp, "Error: Errore durante la creazione della directory %s per l'architettura %s. Errore: %s. I binari per questa architettura verranno inseriti nella parent directory della release.\n", final_target_dir, args[i].arch, strerror(errno));
                        snprintf(final_target_path, sizeof(final_target_path), "%s/%s", target_dir, expected_binary_name);
                    } else {
                        fprintf(log_fp, "Directory %s creata con successo per la nuova release (architettura %s).\n", final_target_dir, args[i].arch);
                        snprintf(final_target_path, sizeof(final_target_path), "%s/%s", final_target_dir, expected_binary_name);
                    }
                } else {
                    fprintf(log_fp, "Directory %s già esistente per la release (architettura %s).\n", final_target_dir, args[i].arch);
                    snprintf(final_target_path, sizeof(final_target_path), "%s/%s", final_target_dir, expected_binary_name);
                }

                if (access(source_bin_path, F_OK) == 0) {
                    if (access(final_target_path, F_OK) == 0) {
                        if (remove(final_target_path) != 0) {
                            fprintf(log_fp, "Error: Errore durante la rimozione del vecchio binario %s per l'architettura %s. Errore: %s\n", final_target_path, args[i].arch, strerror(errno));
                        } else {
                            fprintf(log_fp, "Vecchio binario per l'architettura %s rimosso con successo.\n", args[i].arch);
                        }
                    }

                    if (rename(source_bin_path, final_target_path) != 0) {
                        fprintf(log_fp, "Error: Errore durante lo spostamento del binario %s a %s per l'architettura %s. Errore: %s\n", source_bin_path, final_target_path, args[i].arch, strerror(errno));
                    } else {
                        fprintf(log_fp, "Binario per l'architettura %s spostato con successo in %s.\n", args[i].arch, final_target_path);
                    }
                } else {
                    fprintf(log_fp, "Error: Binario sorgente %s non trovato per l'architettura %s. Spostamento saltato.\n", source_bin_path, args[i].arch);
                }
            }

            fprintf(log_fp, "\n");
            log_time(log_fp);
            fprintf(log_fp, "Build completata per tutte le architetture.\n");

        }/*  else if (round == 0 && initial_check.status == 1 && new_commit.status == 1) {
            // impossibile: initial_check.status == 1 significherebbe che ho avuto errore durante il check_host_dirs,
            // quindi non posso trovarmi qua.
            // Nota: questo scenario potrebbe sembrare essere associato anche a un'esecuzione "vuota" (sono arrivato qua con ancora i valori di
            // inizializzazione). Ma ciò non è possibile perché con round == 0 eseguo check_host_dirs, che lascia 
            // initial_check.status == 1 solo se fallisce, ma in tal caso si uscirebbe dal ciclo e non si potrebbe mai arrivare qua.

        } else if (round == 0 && initial_check.status == 0 && new_commit.status == 1) {
            // Al primo avvio ho trovato già tutto clonato e ho provato a pullare ma c'è stato un errore.
            // In questo caso devo fare break (un errore nel pull è critico, non posso continuare a girare il demone)
            fprintf(log_fp, "Errore durante check_new_commit al primo avvio: sono state trovate repo già clonate ma il pull ha fallito. Esco dal demone...\n");
            break;

        } else if (round > 0 && initial_check.status == 1 && new_commit.status == 1) {
            // impossibile: initial_check.status == 1 significherebbe che ho avuto errore durante il check_host_dirs,
            // quindi non posso trovarmi qua. Poi oltretutto round > 0, in questo contesto, è paradossale

        } else if (round > 0 && initial_check.status == 0 && new_commit.status == 1) {
            // Al primo avvio avevo registrato che le dirs sull'host erano già presenti e l'host era già correttamente configurato (c'era stato un'interruzione),
            // quindi sono andato avanti con i round (durante i quali anche i check_new_commit sono stati effettuati correttamente).
            // A questo punto però è stato riscontrato un errore durante il pull di questo round e quindi devo uscire dal demone
            fprintf(log_fp, "Errore durante check_new_commit: all'avvio erano state trovate repo già clonate, sono stati effettuati %d round correttamente ma al round %d il pull ha fallito. Esco dal demone...\n", round - 1, round);
            break;

        } else if (round > 0 && initial_check.status == 2 && new_commit.status == 1) {
            // Al primo avvio avevo clonato correttamente le repo, poi sono andato avanti con i round (durante i quali anche i check_new_commit sono stati effettuati correttamente),
            // ma a questo punto ho riscontrato un errore durante check_new_commit; devo quindi uscire dal demone
            fprintf(log_fp, "Errore durante check_new_commit: al primo avvio le repo erano state clonate correttamente, sono stati effettuati %d round correttamente ma al round %d il pull ha fallito. Esco dal demone...\n", round - 1, round);
            break;

        } else if (round == 0 && initial_check.status == 1 && new_commit.status == 0) {
            // impossibile: initial_check.status == 1 significherebbe che ho avuto errore durante il check_host_dirs,
            // quindi non posso trovarmi qua. Anche perché oltretutto new_commit_status == 0 vorrebbe dire che ho eseguit
            // il check_new_commit nonostante l'errore di check_host_dirs

        } else if (round == 0 && initial_check.status == 0 && new_commit.status == 0) {
            // Al primo avvio è stato trovato tutto al suo posto nell'host (le dirs e le repo erano già presenti),
            // è stato quindi effettuato il check_new_commit ma non sono stati trovati nuovi commit.
            // Questo è uno dei casi in cui devo semplicemnte andare avanti

        } else if (round > 0 && initial_check.status == 1 && new_commit.status == 0) {
            // impossibile: initial_check.status == 1 significherebbe che ho avuto errore durante il check_host_dirs,
            // quindi non posso trovarmi qua. Poi oltretutto round > 0 e new_commit_status == 0 in questo contesto, sono paradossali

        } else if (round > 0 && initial_check.status == 0 && new_commit.status == 0) {
            // Al primo avvio era stato trovato tutto al suo posto nell'host (le dirs e le repo erano già presenti),
            // sono stati poi effettuati diversi round e l'ultimo check_new_commit non ha trovato nuovi commit...
            // Anche qui: tutto normale, vado avanti

        } else if (round > 0 && initial_check.status == 2 && new_commit.status == 0) {
            // Al primo avvio avevo clonato correttamente le repo, sono poi andato avanti con i round (durante i quali anche i check_new_commit sono stati effettuati correttamente),
            // e a questo punto ho fatto di nuovo check_new_commit ma non sono stati trovati nuovi commit.
            // Anche in questo caso non devo fare altro che andare avanti

        } */

        // Secondo le considerazioni precedenti è possibile riassumere i possibili esiti dei check nei seguenti casi:
        else if (new_commit.status == 1) {
            fprintf(log_fp, "Error: Errore durante check_new_commit. Esco dal demone...\n");
            break;
        }
        else {                                                  // new_commit.status == 0
            // nessun commit nuovo trovato, vado avanti
        }
        
        if (terminate_daemon_flag) {
            fprintf(log_fp, "Segnale di terminazione ricevuto prima dello sleep e a operazioni terminate, uscita in corso...\n");
            break;
        }

        update_daemon_state(DAEMON_STATE_SLEEPING);
        log_time(log_fp);
        fprintf(log_fp, "Demone in attesa per %d secondi...\n", poll_interval);
        
        // Ciclo di sonno e gestione dei segnali di interruzione
        unsigned int time_left = poll_interval;
        while(time_left > 0) {
            time_left = sleep(time_left);
            if (terminate_daemon_flag) {
                fprintf(log_fp, "Sleep interrotto da segnale di terminazione.\n");
                break; 
            }
            if (time_left > 0) {
                // Se mi rimaneva del tempo per dormire e non ho ricevuto un segnale di stop, allora sono stato disturbato da qualcun altro e quindi ignoro e dormo ancora
                fprintf(log_fp, "Sleep interrotto, %u secondi rimanenti, continuo ad attendere...\n", time_left);
            }
        }

        round++;
    }

    log_time(log_fp);
    fprintf(log_fp, "sshlirp_ci demone terminato.\n");
    fclose(log_fp);

    pthread_mutex_destroy(&chroot_setup_mutex);

    // Libera la memoria allocata
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