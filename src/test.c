#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "utils/utils.h"
#include "test.h"

int test_sshlirp_bin(thread_args_t *args, char *sshlirp_bin_path, FILE *host_log_fp) {
    // Launch the test script to complete the chroot setup for the test and its execution
    int script_status = 0;

    script_status = execute_script_for_thread(
        args->arch,
        TEST_SCRIPT_PATH,
        sshlirp_bin_path, 
        args->chroot_path,
        args->thread_chroot_vdens_dir, 
        args->thread_log_file, 
        args->thread_chroot_log_file,
        NULL,
        args->sudo_user,
        host_log_fp
    );

    if (script_status != 0) {
        fprintf(host_log_fp, "Error: Error executing test script in %s. Script exit status: %d\n", args->chroot_path, script_status);
        return 1;
    }

    return 0;
}

