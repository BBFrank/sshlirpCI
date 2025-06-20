#ifndef WORKER_INIT_H
#define WORKER_INIT_H

#include "types/types.h"

int setup_chroot(thread_args_t* args, FILE* thread_log_fp);
int check_worker_dirs(thread_args_t* args, FILE* thread_log_fp);
int copy_sources_to_chroot(thread_args_t* args, FILE* thread_log_fp);
int compile_and_verify_in_chroot(thread_args_t* args, FILE* thread_log_fp);
int remove_sources_copy_from_chroot(thread_args_t* args, FILE* thread_log_fp);

#endif // WORKER_INIT_H