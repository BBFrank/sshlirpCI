#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "types/types.h"

// Funzione helper per creare, scrivere, rendere eseguibile e poi rimuovere uno script temporaneo
// Nota: questa funzione viene chiamata sia per git clone che check commit. In generale i valori di ritorno degli script lanciati con system() sono i seguenti:
// 1: errore
// 0: non ho fatto niente (es. non ho nulla da clonare perché la repo esiste già oppure non ho pullato niente di nuovo)
// 2: ho fatto qualcosa (es. ho clonato la repo o ho fatto il pull di un nuovo commit)
// Quando si verificano invece errori esternamente allo script (ma sempre in questa funzione), ritorno -1.
int execute_embedded_script(
    const char* script_content, 
    const char* arg1, 
    const char* arg2, 
    const char* arg3,
    const char* arg4,
    const char* arg5,
    const char* versioning_file,
    FILE* log_fp
) {
    char temp_script_path[] = "/tmp/ci_script_XXXXXX";
    int fd = mkstemp(temp_script_path);
    if (fd == -1) {
        fprintf(log_fp, "mkstemp failed: %s\n", strerror(errno));
        return -1;
   }

    ssize_t to_write = strlen(script_content);
    if (write(fd, script_content, to_write) != to_write) {
        fprintf(log_fp, "write to temp script failed: %s\n", strerror(errno));
        close(fd);
        remove(temp_script_path);
        return -1;
    }
    close(fd);

    if (chmod(temp_script_path, 0700) == -1) {
        fprintf(log_fp, "chmod on temp script failed: %s\n", strerror(errno));
        remove(temp_script_path);
        return -1;
    }

    char command[MAX_COMMAND_LEN];

    if (arg4 == NULL) {
        // Se arg4 è NULL, eseguo lo script con 4 (3 + versioning_file) argomenti in quanto sono sicuro che sarà gitClone
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\"", temp_script_path, arg1, arg2, arg3, versioning_file);
    } else {
        // Altrimenti si tratta di checkCommit (6 argomenti = 5 + versioning_file)
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", temp_script_path, arg1, arg2, arg3, arg4, arg5, versioning_file);
    }

    int status = system(command);
    
    remove(temp_script_path);

    if (status == -1) {
        fprintf(log_fp, "system() call to execute temp script failed");
        return -1;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    // Se lo script non è terminato normalmente, stampo un messaggio di errore
    fprintf(log_fp, "Temporary script terminated abnormally. Status: %d\n", status);
    return -1;
}

// Funzione helper per creare, scrivere, rendere eseguibile e poi rimuovere uno script temporaneo all'interno di un chroot
// Nota: a differenza della funzione precedente, questa ritorna solo 0 (successo) o 1 (errore) in quanto gli script eseguiti (chrootSetup, compile, copySrc, removeSrcCopy)
// non devono restituire valori particolari per l'esecuzione di altre operazioni
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
) {
    char temp_script_path[] = "/tmp/ci_script_XXXXXX";
    int fd = mkstemp(temp_script_path);
    if (fd == -1) {
        fprintf(log_fp, "[Thread %s] mkstemp for %s failed: %s\n", arch, script_name, strerror(errno));
        return 1;
    }

    ssize_t to_write = strlen(script_content);
    if (write(fd, script_content, to_write) != to_write) {
        fprintf(log_fp, "[Thread %s] write to temp script %s failed: %s\n", arch, script_name, strerror(errno));
        close(fd);
        remove(temp_script_path);
        return 1;
    }
    close(fd);

    if (chmod(temp_script_path, 0700) == -1) {
        fprintf(log_fp, "[Thread %s] chmod on temp script %s failed: %s\n", arch, script_name, strerror(errno));
        remove(temp_script_path);
        return 1;
    }

    char command[MAX_COMMAND_LEN];

    if (arg4 == NULL) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\"", temp_script_path, arg1, arg2, arg3);
    } else if (arg5 == NULL) {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\"", temp_script_path, arg1, arg2, arg3, arg4);
    } else {
        snprintf(command, sizeof(command), "%s \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", temp_script_path, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    int status = system(command);
    
    remove(temp_script_path);

    if (status == -1) {
        fprintf(log_fp, "system() call to execute temp script failed");
        return 1;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    fprintf(log_fp, "Temporary script terminated abnormally. Status: %d\n", status);
    return 1;
}

// Funzione per ottenere il percorso della directory padre di un path
char *get_parent_dir(char *path){
    if (path == NULL || strlen(path) == 0) {
        return NULL;
    }

    int size = strlen(path);
    // Rimuove eventuali slash finali, tranne se il path è solo "/"
    while (size > 1 && path[size - 1] == '/') {
        size--;
    }

    int i = size - 1;
    while(i >= 0 && path[i] != '/'){
        i--;
    }

    if (i < 0) { // Nessuno slash trovato (es. "filename")
        // Restituisce "." per la directory corrente, o NULL se si preferisce errore
        char *dir = malloc(2);
        if (!dir) return NULL;
        strcpy(dir, ".");
        return dir;
    }

    if (i == 0 && path[0] == '/') { // Path è tipo "/filename" o "/"
        char *dir = malloc(2);
        if (!dir) return NULL;
        strcpy(dir, "/");
        return dir;
    }
    
    // Path è tipo "foo/bar" o "/foo/bar"
    char *dir = (char *)malloc(i + 1);
    if (!dir) return NULL;
    strncpy(dir, path, i);  
    dir[i] = '\0';
    return dir;  
}