#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "dirmanager.h"
#include "api.h"

int current_backup = 0;
int current_threads = 0;


bool process_client_request(Message* msg) {
    switch (msg->opcode) {
        case 1:
            // Processar a conexão
            return false;

        case 2:
            // Processar desconexão
            return true;
        case 3:
            // Processar subscrição
            return false;

        case 4:
            // Processar cancelamento de subscrição
            return false;

        default:
            return false;
    }
}

void process_client(Message* msg, int fd_register) {
    bool done = false;
    while(!done){
        char buffer[128];

        // Ler pedido do cliente (bloqueante)
        ssize_t bytes_read = read(fd_register, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            perror("Erro ao ler do FIFO de registro");
            break;
        }

        buffer[bytes_read] = '\0'; // Garantir terminação da string

        // Processar o pedido do cliente
        done = process_client_request(buffer);
    }
    return NULL;
}


//------------------------------------------------------------ CODE:
int main(int argc, char* argv[]) {

    if (argc != 5){
        write(STDERR_FILENO, "Usage: ", 7); 
        write(STDERR_FILENO, argv[0], strlen(argv[0]));
        write(STDERR_FILENO, "<dir_path> <threads_limit> <backup_limit> <FIFO_registry>\n", 59);
        return 1;
    }
    
    //definimos os valores das variaveis que controlam o maximo de backups e threads
    int backup_limit = atoi(argv[3]);
    int max_threads = atoi(argv[2]);
    pthread_t threads[max_threads];


    // inicializa threads com todos os elementos a 0
    for (int i = 0; i < max_threads; i++) {
        threads[i] = 0;
    }

    const char *server_fifo = argv[4];
    if (mkfifo(server_fifo, 0666) == -1) {
        perror("Failed to create FIFO");
        return 1;
    }

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        return 1;
    }

    //chama a função principal
    iterates_files(argv[1], backup_limit, max_threads, threads);

    //esperamos que todas as threads terminem
    for (int i = 0; i < max_threads; i++) {
        if (threads[i] != 0) {
            pthread_join(threads[i], NULL);
        }
    }

    //libertamos a memoria alocada da hash table
    kvs_terminate();

    return 0;
}


