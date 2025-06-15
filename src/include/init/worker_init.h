#ifndef WORKER_INIT_H
#define WORKER_INIT_H

#include "types/types.h"

int setup_chroot(thread_args_t* args);
int check_worker_dirs(thread_args_t* args);
int copy_sources_to_chroot(thread_args_t* args);
int compile_and_verify_in_chroot(thread_args_t* args);
int remove_sources_copy_from_chroot(thread_args_t* args);

#endif // WORKER_INIT_H