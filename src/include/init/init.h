#ifndef INIT_H
#define INIT_H

#include "types/types.h"

// Dichiarazioni delle funzioni da init.c
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
    int* poll_interval);

int check_host_dirs(
    char* main_dir, 
    char* target_dir, 
    char* sshlirp_source_dir, 
    char* libslirp_source_dir, 
    char* log_file, 
    char* sshlirp_repo_url, 
    char* libslirp_repo_url, 
    char* thread_log_dir);

int check_new_commit(
    char* sshlirp_source_dir, 
    char* sshlirp_repo_url, 
    char* libslirp_source_dir, 
    char* libslirp_repo_url, 
    char* log_file);

#endif // INIT_H