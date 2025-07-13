#include <stdio.h>
#include <stdlib.h>
#include <execs.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "types/types.h"

// Helper function to create, write, make executable, and then remove a temporary script
// Note: this function is called for both git clone and check commit. In general, the return values of scripts launched with system_safes() are as follows:
// 1: error
// 0: I did nothing (e.g., I have nothing to clone because the repo already exists or I haven't pulled anything new)
// 2: I did something (e.g., I cloned the repo or pulled a new commit)
// When errors occur outside the script (but still in this function), I return -1.
int execute_embedded_script(
    const char* script_path,
    const char* arg1, 
    const char* arg2, 
    const char* arg3,
    const char* arg4,
    const char* arg5,
    const char* versioning_file,
    FILE* log_fp
) {
    if (chmod(script_path, 0700) == -1) {
        fprintf(log_fp, "Failed to make script executable: %s\n", strerror(errno));
        return 1;
    }
    char command[MAX_COMMAND_LEN];

    if (strcmp(script_path, GIT_CLONE_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, versioning_file);
    } else if (strcmp(script_path, CHECK_COMMIT_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, arg4, arg5, versioning_file);
    } else {
        fprintf(log_fp, "Unknown script path: %s\n", script_path);
        return 1;
    }

    int status = system_safe(command);

    if (status == -1) {
        fprintf(log_fp, "system_safe() call to execute temp script failed");
        return 1;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    // If the script did not terminate normally, print an error message
    fprintf(log_fp, "Script terminated abnormally. Status: %d\n", status);
    return 1;
}

// Helper function to create, write, make executable, and then remove a temporary script inside a chroot
// Note: unlike the previous function, this one only returns 0 (success) or 1 (error) as the executed scripts (chrootSetup, compile, copySrc, removeSrcCopy)
// do not need to return special values for the execution of other operations
int execute_embedded_script_for_thread(
    const char* arch,
    const char* script_path,
    const char* arg1,
    const char* arg2,
    const char* arg3,
    const char* arg4,
    const char* arg5,
    const char* arg6,
    FILE* log_fp
) {
    if (chmod(script_path, 0700) == -1) {
        fprintf(log_fp, "[Thread %s] Failed to make script executable: %s\n", arch, strerror(errno));
        return 1;
    }
    char command[MAX_COMMAND_LEN];

    if (strcmp(script_path, CHROOT_SETUP_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3);
    } else if (strcmp(script_path, COPY_SOURCE_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, arg4);
    } else if (strcmp(script_path, COMPILE_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, arg4, arg5, arg6);
    } else if (strcmp(script_path, REMOVE_SOURCE_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, arg4);
    } else if (strcmp(script_path, MODIFY_VDENS_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\"", script_path, arg1, arg2);
    } else if (strcmp(script_path, TEST_SCRIPT_PATH) == 0) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", script_path, arg1, arg2, arg3, arg4, arg5);
    } else {
        fprintf(log_fp, "[Thread %s] Unknown script path: %s\n", arch, script_path);
        return 1;
    }

    int status = system_safe(command);

    if (status == -1) {
        fprintf(log_fp, "system_safe() call to execute temp script failed");
        return 1;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    fprintf(log_fp, "[Thread %s] Script terminated abnormally. Status: %d\n", arch, status);
    return 1;
}

// Function to get the path of the parent directory of a path
char *get_parent_dir(char *path){
    if (path == NULL || strlen(path) == 0) {
        return NULL;
    }

    int size = strlen(path);
    // Removes any trailing slashes, unless the path is just "/"
    while (size > 1 && path[size - 1] == '/') {
        size--;
    }

    int i = size - 1;
    while(i >= 0 && path[i] != '/'){
        i--;
    }

    if (i < 0) { // No slash found (e.g. "filename")
        // Returns "." for the current directory, or NULL if an error is preferred
        char *dir = malloc(2);
        if (!dir) return NULL;
        strcpy(dir, ".");
        return dir;
    }

    if (i == 0 && path[0] == '/') { // Path is like "/filename" or "/"
        char *dir = malloc(2);
        if (!dir) return NULL;
        strcpy(dir, "/");
        return dir;
    }
    
    // Path is like "foo/bar" or "/foo/bar"
    char *dir = (char *)malloc(i + 1);
    if (!dir) return NULL;
    strncpy(dir, path, i);  
    dir[i] = '\0';
    return dir;  
}