#include <limits.h>
//#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "dirmanager.h"

int current_backup = 0;
int current_threads = 0;


//------------------------------------------------------------ CODE:
int main(int argc, char* argv[]) {

    if (argc != 4){
        write(STDERR_FILENO, "Usage: ", 7); 
        write(STDERR_FILENO, argv[0], strlen(argv[0]));
        write(STDERR_FILENO, "<dir_path> <backup_limit> <threads_limit>\n", 42);
        return 1;
    }
    
    //definimos os valores das variaveis que controlam o maximo de backups e threads
    int backup_limit = atoi(argv[2]);
    int max_threads = atoi(argv[3]);
    pthread_t threads[max_threads];

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        return 1;
    }

    iterates_files(argv[1], backup_limit, max_threads, threads);

    //esperamos que todas as threads terminem
    for (int i = 0; i < max_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    kvs_terminate();

    return 0;
}


