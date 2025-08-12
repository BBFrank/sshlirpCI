#ifndef UTILS_H
#define UTILS_H

#include "types/types.h"
#include <stdio.h>

int execute_script(
    const char* script_path,
    const char* arg1, 
    const char* arg2, 
    const char* arg3, 
    const char* arg4, 
    const char* arg5,
    const char* versioning_file,
    FILE* log_fp
);

int execute_script_for_thread(
    const char* arch,
    const char* script_path,
    const char* arg1,
    const char* arg2,
    const char* arg3,
    const char* arg4,
    const char* arg5,
    const char* arg6,
    const int sudo_user,
    FILE* log_fp
);

char *get_parent_dir(char *path);

#endif // UTILS_H


