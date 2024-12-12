// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <time.h>
// #include <dirent.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <pthread.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "kvs.h"
#include "constants.h"
#include "operations.h"
#include "dirmanager.h"

static struct HashTable* kvs_table = NULL;


//------------------------------------------------------------ CODE:
/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
    return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}


int kvs_init() {
    if (kvs_table /*!= NULL*/) {
        fprintf(stderr, "KVS state has already been initialized\n");
        return 1;
    }

    kvs_table = create_hash_table();
    return kvs_table == NULL;
}


int kvs_terminate() {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    free_table(kvs_table);
    kvs_table = NULL;
    return 0;
}


int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    for (size_t i = 0; i < num_pairs; i++) {
        if (write_pair(kvs_table, keys[i], values[i]) != 0) {
            fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
        }
    }

    return 0;
}


int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
    char temp[MAX_STRING_SIZE];

    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    //ordena alfabeticamente
    for (size_t i = 0; i < num_pairs - 1; i++) {
        for (size_t j = i + 1; j < num_pairs; j++) {
            if (strcmp(keys[i], keys[j]) > 0) {
                strcpy(temp, keys[i]);
                strcpy(keys[i], keys[j]);
                strcpy(keys[j], temp);
            }
        }
    }

    write(fd_out, "[", 1);
    for (size_t i = 0; i < num_pairs; i++) {
        char *result = read_pair(kvs_table, keys[i]);

        write(fd_out, "(", 1);
        write(fd_out, keys[i], strlen(keys[i]));
        if (result == NULL) {
            write(fd_out, ",KVSERROR", 9);
        } else {
            write(fd_out, ",", 1);
            write(fd_out, result, strlen(result));
        }
        write(fd_out, ")", 1);
        free(result);
        result = NULL;
    }
    write(fd_out, "]\n", 2);

    return 0;
}


int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }
    int aux = 0;

    for (size_t i = 0; i < num_pairs; i++) {
        if (delete_pair(kvs_table, keys[i]) != 0) {
            if (!aux) {
                write(fd_out, "[", 1);
                aux = 1;
            }
                write(fd_out, "(", 1);
                write(fd_out, keys[i], strlen(keys[i]));
                write(fd_out, ",KVSMISSING)", 12);
        }
    }
    if (aux) {
        write(fd_out, "]\n", 2);
    }

    return 0;
}


void kvs_show(int fd_out) {
    pthread_mutex_lock(&global_lock);

    for (int i = 0; i < TABLE_SIZE; i++) {
        pthread_mutex_lock(&kvs_table->kvs_lock[i]);
        for(KeyNode *keyNode = kvs_table->table[i]; keyNode != NULL; keyNode = keyNode->next) {
            write(fd_out, "(", 1);
            write(fd_out, keyNode->key, strlen(keyNode->key));
            write(fd_out, ", ", 2);
            write(fd_out, keyNode->value, strlen(keyNode->value));
            write(fd_out, ")\n", 2);            
        }
        pthread_mutex_unlock(&kvs_table->kvs_lock[i]);
    }
    pthread_mutex_unlock(&global_lock);
}


int kvs_backup(int backup_file_count, char *file_path) {
    char backup_file[MAX_JOB_FILE_NAME_SIZE];

    //Cria o fork e continua a executar o pai e o filho 
    pid_t pid = fork();   

    if (pid < 0){
        fprintf(stderr, "Failed to fork for backup\n");
        return 1;
    }  

    backup_file_count++;

    snprintf(backup_file, MAX_JOB_FILE_NAME_SIZE, "%.*s", MAX_JOB_FILE_NAME_SIZE - 5, file_path);
    char *extension = strstr(backup_file, ".job");
    if (extension) {
        strcpy(extension, "\0");
    }
    sprintf(backup_file, "-%d.bck", backup_file_count);

    

    // Processo filho
    if (pid == 0) { 
        //cria o ficheiro de backup 
        int fd_backup = open(backup_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        for (int i = 0; i < TABLE_SIZE; i++) {
            for(KeyNode *keyNode = kvs_table->table[i]; keyNode != NULL; keyNode = keyNode->next) {
                write(fd_backup, "(", 1);
                write(fd_backup, keyNode->key, strlen(keyNode->key));
                write(fd_backup, ", ", 2);
                write(fd_backup, keyNode->value, strlen(keyNode->value));
                write(fd_backup, ")\n", 2);                
            }
        }

        close(fd_backup);
        _exit(0);//Chamada para fechar o processo filho
    } 
    // Processo pai
    else if (pid > 0) { 
        //não faz nada, apenas retorna onde foi chamado
        return 0;
    }
    else { 
        fprintf(stderr, "Failed to fork for backup\n");
    }
    return 0;
}
  

void kvs_wait(unsigned int delay_ms) {
    struct timespec delay = delay_to_timespec(delay_ms);
    nanosleep(&delay, NULL);
}