#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "init/init.h"
#include "scripts/git_clone_script.h"
#include "scripts/check_commit_script.h"
#include "utils/utils.h"

// Funzione per caricare le architetture dal file di configurazione
static void load_architectures(char** archs_list, int* num_archs_out) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error opening config file");
        *num_archs_out = 0;
        return;
    }

    char line[MAX_CONFIG_LINE_LEN];
    char* architectures_val_str = NULL;
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, CONFIG_ARCH_KEY, strlen(CONFIG_ARCH_KEY)) == 0) {
            char* value_start = line + strlen(CONFIG_ARCH_KEY);
            value_start[strcspn(value_start, "\n")] = 0;

            architectures_val_str = strdup(value_start);
            if (!architectures_val_str) {
                perror("strdup failed for architectures_val_str");
                fclose(fp);
                *num_archs_out = 0;
                return;
            }
            break;
        }
    }
    fclose(fp);

    if (!architectures_val_str || strlen(architectures_val_str) == 0) {
        fprintf(stderr, "%s key not found or empty in config file.\n", CONFIG_ARCH_KEY);
        free(architectures_val_str);
        *num_archs_out = 0;
        return;
    }

    // Primo passaggio: contare le architetture
    char* temp_str_for_count = strdup(architectures_val_str);
    if (!temp_str_for_count) {
        perror("strdup failed for temp_str_for_count");
        free(architectures_val_str);
        free(temp_str_for_count);
        *num_archs_out = 0;
        return;
    }

    char* token = strtok(temp_str_for_count, ",");
    while (token) {
        if (strlen(token) > 0) {
            count++;
        }
        token = strtok(NULL, ",");
    }
    free(temp_str_for_count);

    if (count == 0) {
        fprintf(stderr, "No architectures found for %s key.\n", CONFIG_ARCH_KEY);
        free(architectures_val_str);
        *num_archs_out = 0;
        return;
    }

    // Secondo passaggio: popolare l'array
    token = strtok(architectures_val_str, ",");
    int current_arch = 0;
    while (token && current_arch < count) {
        archs_list[current_arch] = strdup(token);
        if (!archs_list[current_arch]) {
            perror("strdup failed for an architecture token");
            free(architectures_val_str);
            *num_archs_out = 0;
            return;
        }
        current_arch++;
        token = strtok(NULL, ",");
    }

    free(architectures_val_str);

    *num_archs_out = current_arch;
    return;
}

// Funzione per caricare un URL da una chiave specifica
static void load_path(const char* key, char* path) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error opening config file");
        path[0] = '\0';
        return;
    }

    char line[MAX_CONFIG_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            char* value_start = line + strlen(key);
            value_start[strcspn(value_start, "\n")] = 0;
            strncpy(path, value_start, MAX_CONFIG_ATTR_LEN - 1);
            path[MAX_CONFIG_ATTR_LEN - 1] = '\0';
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    path[0] = '\0';
}

// Funzione per caricare l'intervallo di polling
static void load_poll_interval(int* poll_interval) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error opening config file");
        *poll_interval = 0;
        return;
    }

    char line[MAX_CONFIG_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, CONFIG_INTERVAL_KEY, strlen(CONFIG_INTERVAL_KEY)) == 0) {
            char* value_start = line + strlen(CONFIG_INTERVAL_KEY);
            *poll_interval = atoi(value_start);
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    *poll_interval = 0;
}

// Funzione per liberare la memoria allocata per la lista delle architetture
static void free_architectures(char** archs_list, int num_archs) {
    for (int i = 0; i < num_archs; i++) {
        free(archs_list[i]);
    }
    free(archs_list);
}

// Funzione per liberare le risorse allocate
static void free_resources(
    char** archs, 
    int* num_archs, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* main_dir, 
    char* target_dir, 
    char* sshlirp_source_dir, 
    char* libslirp_source_dir, 
    char* log_file, 
    char* thread_chroot_target_dir,
    char* thread_chroot_log_file,
    char* thread_log_dir) {
    
    free_architectures(archs, *num_archs);
    free(sshlirp_repo_url);
    free(libslirp_repo_url);
    free(main_dir);
    free(target_dir);
    free(sshlirp_source_dir);
    free(libslirp_source_dir);
    free(log_file);
    free(thread_chroot_target_dir);
    free(thread_chroot_log_file);
    free(thread_log_dir);
}

// Funzione per caricare le variabili di configurazione
int conf_vars_loader(
    char** archs, 
    int* num_archs, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* main_dir, 
    char* target_dir, 
    char* sshlirp_source_dir, 
    char* libslirp_source_dir, 
    char* log_file, 
    char* thread_chroot_target_dir,
    char* thread_chroot_log_file,
    char* thread_log_dir, 
    int* poll_interval) {

        load_architectures(archs, num_archs);
        if (*num_archs == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "No architectures found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_SSHLIRP_KEY, sshlirp_repo_url);
        if (strlen(sshlirp_repo_url) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "SSHLIRP_REPO_URL not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_LIBSLIRP_KEY, libslirp_repo_url);
        if (strlen(libslirp_repo_url) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "LIBSLIRP_REPO_URL not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_MAINDIR_KEY, main_dir);
        if (strlen(main_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "TARGET_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_TARGETDIR_KEY, target_dir);
        if (strlen(target_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "TARGET_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_SSHLIRP_SOURCE_DIR_KEY, sshlirp_source_dir);
        if (strlen(sshlirp_source_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "SSHLIRP_SOURCE_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_LIBSLIRP_SOURCE_DIR_KEY, libslirp_source_dir);
        if (strlen(libslirp_source_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "LIBSLIRP_SOURCE_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_LOG_KEY, log_file);
        if (strlen(log_file) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "LOG_FILE not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_THREAD_CHROOT_TARGET_DIR_KEY, thread_chroot_target_dir);
        if (strlen(thread_chroot_target_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "THREAD_CHROOT_TARGET_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_THREAD_CHROOT_LOG_FILE_KEY, thread_chroot_log_file);
        if (strlen(thread_chroot_log_file) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "THREAD_CHROOT_LOG_FILE not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_THREAD_LOG_DIR_KEY, thread_log_dir);
        if (strlen(thread_log_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "THREAD_LOG_DIR not found in configuration.\n");
            return 1;
        }
        load_poll_interval(poll_interval);
        if (*poll_interval <= 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, main_dir, target_dir, sshlirp_source_dir, libslirp_source_dir, log_file, thread_chroot_target_dir, thread_chroot_log_file, thread_log_dir);
            fprintf(stderr, "POLL_INTERVAL not found or invalid in configuration.\n");
            return 1;
        }
        return 0;
    }

// Funzione per verificare se le directory host esistono o crearle e clonare i repository
int check_host_dirs(char* target_dir, char* sshlirp_source_dir, char* libslirp_source_dir, char* log_file, char* sshlirp_repo_url, char* libslirp_repo_url, char* thread_log_dir, FILE* log_fp) {

    // 1. Controllo l'esistenza e, se necessario, creo le directories e il file di log nella macchina host

    // es: /home/sshlirpCI/thread-binaries
    if (access(target_dir, F_OK) == -1) {
        if (mkdir(target_dir, 0755) == -1) {
            fprintf(log_fp, "Error creating target directory: %s\n", strerror(errno));
            return 1;
        }
    }

    // es: /home/sshlirpCI/sshlirp
    if (access(sshlirp_source_dir, F_OK) == -1) {
        if (mkdir(sshlirp_source_dir, 0755) == -1) {
            fprintf(log_fp, "Error creating SSHLIRP source directory: %s\n", strerror(errno));
            return 1;
        }
    }

    // es: /home/sshlirpCI/libslirp
    if (access(libslirp_source_dir, F_OK) == -1) {
        if (mkdir(libslirp_source_dir, 0755) == -1) {
            fprintf(log_fp, "Error creating LIBSLIRP source directory: %s\n", strerror(errno));
            return 1;
        }
    }

    // es: /home/sshlirpCI/log/threads (sono sicuro che la directory log esista già, creata in main.c)
    if (access(thread_log_dir, F_OK) == -1) {
        if (mkdir(thread_log_dir, 0755) == -1) {
            fprintf(log_fp, "Error creating thread log directory: %s\n", strerror(errno));
            return 1;
        }
    }

    // 2. Clono le repo nei rispettivi percorsi -> lancio lo script incorporato gitClone.sh
    int script_status;

    script_status = execute_embedded_script(git_clone_script_content, sshlirp_repo_url, sshlirp_source_dir, log_file, NULL, NULL, log_fp);
    if (script_status != 0) {
        fprintf(log_fp, "Error cloning sshlirp repository via embedded script. Script exit status: %d\n", script_status);
        return 1;
    }

    script_status = execute_embedded_script(git_clone_script_content, libslirp_repo_url, libslirp_source_dir, log_file, NULL, NULL, log_fp);
    if (script_status != 0) {
        fprintf(log_fp, "Error cloning libslirp repository via embedded script. Script exit status: %d\n", script_status);
        return 1;
    }

    return 0;
}

int check_new_commit(char* sshlirp_source_dir, char* sshlirp_repo_url, char* libslirp_source_dir, char* libslirp_repo_url, char* log_file, FILE* log_fp) {
    int script_status;

    script_status = execute_embedded_script(check_commit_script_content, sshlirp_source_dir, sshlirp_repo_url, libslirp_source_dir, libslirp_repo_url, log_file, log_fp);

    if (script_status == 1 || script_status == -1) {
        fprintf(log_fp, "Error checking for new commits. Script exit status: %d\n", script_status);
        return 1;
    }

    if (script_status == 2) {
        fprintf(log_fp, "New commits for sshlirp detected and pulled.\n");
        return 2;
    }

    if (script_status == 0) {
        fprintf(log_fp, "No new commits found for sshlirp.\n");
        return 0;
    }

    fprintf(log_fp, "Unexpected commit check status from script: %d\n", script_status);
    return 1;
}