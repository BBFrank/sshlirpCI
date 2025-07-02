#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "utils/utils.h"
#include "scripts/test_script.h"
#include "scripts/git_clone_script.h"
#include "test.h"

int test_sshlirp_bin(thread_args_t *args, char *sshlirp_bin_path, FILE *host_log_fp) {
    // Lancio lo script di test per completare il setup del chroot per il test e la sua esecuzione
    int script_status = 0;

    script_status = execute_embedded_script_for_thread(
        args->arch,
        test_script_content, 
        "test_script", 
        sshlirp_bin_path, 
        args->chroot_path,
        args->thread_chroot_vdens_dir, 
        args->thread_log_file, 
        args->thread_chroot_log_file,
        NULL, 
        host_log_fp
    );
    if (script_status != 0) {
        fprintf(host_log_fp, "Error: Errore durante l'esecuzione dello script di test in %s. Script exit status: %d\n", args->chroot_path, script_status);
        return 1;
    }

    return 0;
}

