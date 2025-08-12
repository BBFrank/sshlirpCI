#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "init/init.h"
#include "utils/utils.h"

// Function to load architectures from the configuration file
static void load_architectures(char** archs_list, int* num_archs_out) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error: Error opening config file, please check the path in /src/include/types/types.h, row 6.");
        *num_archs_out = 0;
        return;
    }

    char line[MIN_CONFIG_ATTR_LEN];
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

    // First pass: count the architectures
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

    // Second pass: populate the array
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

// Function to load a URL from a specific key
static void load_path(const char* key, char* path) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error: Error opening config file, please check the path in /src/include/types/types.h, row 6.");
        path[0] = '\0';
        return;
    }

    char line[MIN_CONFIG_ATTR_LEN];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            char* value_start = line + strlen(key);
            value_start[strcspn(value_start, "\n")] = 0;
            strncpy(path, value_start, MIN_CONFIG_ATTR_LEN - 1);
            path[MIN_CONFIG_ATTR_LEN - 1] = '\0';
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    path[0] = '\0';
}

// Function to load the polling interval
static void load_poll_interval(int* poll_interval) {
    FILE* fp = fopen(DEFAULT_CONFIG_PATH, "r");
    if (!fp) {
        perror("Error: Error opening config file, please check the sshlirpCI source path in /src/include/types/types.h, row 4.");
        *poll_interval = 0;
        return;
    }

    char line[CONFIG_ATTR_LEN];
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

// Function to free the memory allocated for the list of architectures
static void free_architectures(char** archs_list, int num_archs) {
    for (int i = 0; i < num_archs; i++) {
        free(archs_list[i]);
    }
    free(archs_list);
}

// Function to free allocated resources
static void free_resources(
    char** archs, 
    int* num_archs, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* vdens_repo_url,
    char* main_dir,
    char* target_dir,
    char* log_file) {
    
    free_architectures(archs, *num_archs);
    free(sshlirp_repo_url);
    free(libslirp_repo_url);
    free(vdens_repo_url);
    free(main_dir);
    free(target_dir);
    free(log_file);
}

// Function to load configuration variables
int conf_vars_loader(
    char** archs, 
    int* num_archs, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* vdens_repo_url,
    char* main_dir,
    char* target_dir,
    char* log_file,
    int* poll_interval) {

        load_architectures(archs, num_archs);
        if (*num_archs == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "No architectures found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_SSHLIRP_KEY, sshlirp_repo_url);
        if (strlen(sshlirp_repo_url) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "SSHLIRP_REPO_URL not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_LIBSLIRP_KEY, libslirp_repo_url);
        if (strlen(libslirp_repo_url) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "LIBSLIRP_REPO_URL not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_MAINDIR_KEY, main_dir);
        if (strlen(main_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "MAINDIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_TARGETDIR_KEY, target_dir);
        if (strlen(target_dir) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "TARGET_DIR not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_VDENS_REPO_URL_KEY, vdens_repo_url);
        if (strlen(vdens_repo_url) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "VDENS_REPO_URL not found in configuration.\n");
            return 1;
        }
        load_path(CONFIG_LOG_KEY, log_file);
        if (strlen(log_file) == 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "LOG_FILE not found in configuration.\n");
            return 1;
        }
        load_poll_interval(poll_interval);
        if (*poll_interval <= 0) {
            free_resources(archs, num_archs, sshlirp_repo_url, libslirp_repo_url, vdens_repo_url, main_dir, target_dir, log_file);
            fprintf(stderr, "POLL_INTERVAL not found or invalid in configuration.\n");
            return 1;
        }
        return 0;
    }

// Function to get the last release from the versioning file, save it in a commit_status_t structure and print logs to a log_fp
static int get_last_release(const char* versioning_file, commit_status_t* result, FILE* log_fp) {
    FILE* versioning_fp = fopen(versioning_file, "r");
    if (!versioning_fp) {
        fprintf(log_fp, "Error: Error opening versioning file: %s\n", strerror(errno));
        return 1;
    }
    char line[MAX_VERSIONING_LINE_LEN];
    char last_line[MAX_VERSIONING_LINE_LEN] = "";

    while (fgets(line, sizeof(line), versioning_fp) != NULL) {
        strcpy(last_line, line);
    }

    if (strlen(last_line) > 0) {
        last_line[strcspn(last_line, "\n")] = 0;
        result->new_release = strdup(last_line);
    } else {
        result->new_release = strdup("unstable");
    }
    fclose(versioning_fp);
    return 0;
}

// Function to check if host directories exist or create them and clone repositories
// Note: this function launches a script and based on its return values, can return the following values:
// 1: error
// 0: clone was not performed, as the repo already existed
// 2: clone was performed
commit_status_t check_host_dirs(
    char* target_dir, 
    char* sshlirp_source_dir, 
    char* libslirp_source_dir, 
    char* vdens_source_dir,
    char* log_file, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* vdens_repo_url,
    char* thread_log_dir, 
    FILE* log_fp, 
    char* versioning_file
) {
    commit_status_t result = {1, NULL};
    // 1. Check for existence and, if necessary, create the directories and the log file on the host machine

    // ex: /home/sshlirpCI/thread-binaries
    if (access(target_dir, F_OK) == -1) {
        if (mkdir(target_dir, 0755) == -1) {
            fprintf(log_fp, "Error: Error creating target directory: %s\n", strerror(errno));
            return result;
        }
    }

    // ex: /home/sshlirpCI/sshlirp
    if (access(sshlirp_source_dir, F_OK) == -1) {
        if (mkdir(sshlirp_source_dir, 0755) == -1) {
            fprintf(log_fp, "Error: Error creating SSHLIRP source directory: %s\n", strerror(errno));
            return result;
        }
    }

    // ex: /home/sshlirpCI/libslirp
    if (access(libslirp_source_dir, F_OK) == -1) {
        if (mkdir(libslirp_source_dir, 0755) == -1) {
            fprintf(log_fp, "Error: Error creating LIBSLIRP source directory: %s\n", strerror(errno));
            return result;
        }
    }

    // ex: /home/sshlirpCI/vdens (only if testing is enabled will I create the vdens directory)
#ifdef TEST_ENABLED
    if (access(vdens_source_dir, F_OK) == -1) {
        if (mkdir(vdens_source_dir, 0755) == -1) {
            fprintf(log_fp, "Error: Error creating VDENS source directory: %s\n", strerror(errno));
            return result;
        }
    }
#endif

    // ex: /home/sshlirpCI/log/threads (I'm sure the log directory already exists, created in main.c)
    if (access(thread_log_dir, F_OK) == -1) {
        if (mkdir(thread_log_dir, 0755) == -1) {
            fprintf(log_fp, "Error: Error creating thread log directory: %s\n", strerror(errno));
            return result;
        }
    }

    // 2. Clone the repos in their respective paths -> launch the embedded gitClone.sh script
    int script_status;

    script_status = execute_script(
        GIT_CLONE_SCRIPT_PATH,
        sshlirp_repo_url, 
        sshlirp_source_dir, 
        log_file, 
        NULL, 
        NULL, 
        versioning_file, 
        log_fp);
    if (script_status == 1) {
        fprintf(log_fp, "Error: Error cloning sshlirp repository via embedded script. Script exit status: %d\n", script_status);
        return result;
    }

    // Note: in the case of git clone of libslirp I don't pass the versioning_file, because otherwise the script would write the latest version of libslirp to it
    script_status = execute_script(
        GIT_CLONE_SCRIPT_PATH,
        libslirp_repo_url, 
        libslirp_source_dir, 
        log_file, 
        NULL, 
        NULL, 
        "", 
        log_fp);
    if (script_status == 1) {
        fprintf(log_fp, "Error: Error cloning libslirp repository via embedded script. Script exit status: %d\n", script_status);
        return result;
    }

#ifdef TEST_ENABLED
    // Clone the vdens repo only if testing is enabled and I don't pass the versioning file
    script_status = execute_script(
        GIT_CLONE_SCRIPT_PATH,
        vdens_repo_url, 
        vdens_source_dir, 
        log_file, 
        NULL, 
        NULL, 
        "", 
        log_fp);
    if (script_status == 1) {
        fprintf(log_fp, "Error: Error cloning vdens repository via embedded script. Script exit status: %d\n", script_status);
        return result;
    }
#endif

    // Verify the sshlirp versioning file
    if (get_last_release(versioning_file, &result, log_fp) != 0) {
        return result;
    }

    result.status = script_status;
    return result;
}

// Function to check for (and possibly pull) new commits in the sshlirp repo from the remote repo
// Note: this function calls a script and, similarly to the check_host_dirs function, can return the following values:
// 1: error
// 0: no new commits were found, the repo is already up to date
// 2: new commits were found, the repo has been updated
commit_status_t check_new_commit(char* sshlirp_source_dir, char* sshlirp_repo_url, char* libslirp_source_dir, char* libslirp_repo_url, char* log_file, FILE* log_fp, char* versioning_file) {
    commit_status_t result = {1, NULL};
    int script_status = execute_script(
        CHECK_COMMIT_SCRIPT_PATH,
        sshlirp_source_dir, 
        sshlirp_repo_url, 
        libslirp_source_dir, 
        libslirp_repo_url, 
        log_file, 
        versioning_file, 
        log_fp);

    if (script_status == 1) {
        fprintf(log_fp, "Error: Error checking for new commits. Script exit status: %d\n", script_status);
        return result;
    }

    if (script_status == 2) {
        fprintf(log_fp, "New commits for sshlirp detected and pulled.\n");

        // Read the versioning file to get the latest release
        if (get_last_release(versioning_file, &result, log_fp) != 0) {
            return result;
        }

        result.status = script_status;
        return result;
    }

    if (script_status == 0) {
        fprintf(log_fp, "No new commits found for sshlirp.\n");
        result.status = script_status;
        return result;
    }

    fprintf(log_fp, "Unexpected commit check status from script: %d\n", script_status);
    return result;
}