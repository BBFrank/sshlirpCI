#ifndef UTILS_H
#define UTILS_H

#include "types/types.h"

int execute_embedded_script(
    const char* script_content, 
    const char* arg1, 
    const char* arg2, 
    const char* arg3, 
    const char* arg4, 
    const char* arg5,
    FILE* log_fp
);

int execute_embedded_script_for_thread(
    const char* arch,
    const char* script_content,
    const char* script_name,
    const char* arg1,
    const char* arg2,
    const char* arg3,
    const char* arg4,
    const char* arg5,
    const char* arg6,
    FILE* log_fp
);

char *get_parent_dir(char *path);

#endif // UTILS_H


