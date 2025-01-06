#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "dirmanager.h"
#include "../client/api.h"

int current_backup = 0;
int current_threads = 0;

pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct ClientNode {
    int client_fd;                // File descriptor do cliente
    struct ClientNode* next;      // Próximo cliente na fila
} ClientNode;

typedef struct ClientQueue {
    ClientNode* front;            // Início da fila
    ClientNode* rear;             // Fim da fila
    pthread_mutex_t mutex;        // Mutex para sincronização
    pthread_cond_t cond;          // Condição para notificar threads
} ClientQueue;

// Inicializar a fila de clientes
void init_client_queue(ClientQueue* queue) {
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Enfileirar um cliente na fila
void enqueue_client(ClientQueue* queue, int client_fd) {
    ClientNode* new_node = (ClientNode*)malloc(sizeof(ClientNode));
    new_node->client_fd = client_fd;
    new_node->next = NULL;

    pthread_mutex_lock(&queue->mutex);
    if (queue->rear == NULL) {
        queue->front = queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }
    pthread_cond_signal(&queue->cond); // Notificar threads esperando na fila
    pthread_mutex_unlock(&queue->mutex);
}

// Desenfileirar um cliente da fila
int dequeue_client(ClientQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->front == NULL) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    ClientNode* temp = queue->front;
    int client_fd = temp->client_fd;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&queue->mutex);
    return client_fd;
}

// Verificar se a fila está vazia
int is_client_queue_empty(ClientQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    int is_empty = (queue->front == NULL);
    pthread_mutex_unlock(&queue->mutex);
    return is_empty;
}

// Finalizar a fila de clientes
void destroy_client_queue(ClientQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->front != NULL) {
        ClientNode* temp = queue->front;
        queue->front = queue->front->next;
        free(temp);
    }
    queue->rear = NULL;
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

bool process_client_request(Message* msg) {
    switch (msg->opcode) {
        // case 1:
        //     // Processar a conexão
        //     return false;

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

//Le a mensagem do cliente e processa-a
void process_client(int fd_register) {
    bool done = false;
    while(!done){
        Message msg;

        // Ler pedido do cliente (bloqueante)
        ssize_t bytes_read = read(fd_register, &msg, sizeof(msg));
        if (bytes_read <= 0) {
            perror("Erro ao ler do FIFO de registro");
            break;
        }

        // Processar o pedido do cliente
        done = process_client_request(&msg);
    }
}

// void *thread_client_queue(void *arg) {
//     ClientQueue *local_q = (ClientQueue *)arg;

//     //loop infinito que irá parar quando a queue estiver vazia
//     while (1) {
//         pthread_mutex_lock(&client_lock);

//         //caso em que a queue está vazia
//         if (is_client_queue_empty(local_q)) {
//             pthread_mutex_unlock(&client_lock);
//             return NULL;
//         }

//         //retira um ficheiro da queue
//         int client_fd = dequeue(local_q);
//         pthread_mutex_unlock(&client_lock);

//         if (client_fd == NULL) {
//             return NULL;
//         }

//         //chama a função de processamento do ficheiro
//         process_client(client_fd);
//     }
// }

void master_task(Queue* pool_jobs, ClientQueue* pool_clients, const char* server_fifo,
 int max_threads, int backup_limit, pthread_t *threads) {
    // pthread_t client_threads[S];
    int fd_register = open(server_fifo, O_RDONLY);
    if (fd_register == -1) {
        perror("Erro ao abrir FIFO de registro");
        return;
    }

    // //inicializa as threads para os clientes
    // for (int i = 0; i < S; i++){
    //     if (pthread_create(&client_threads[i], NULL, thread_client_queue, 
    //                                          (void *) &fd_register)) {
    //         fprintf(stderr, "Failed to create thread\n");
    //         continue;
    //     }   
    // }

    //inicializa as threads para a função thread_queue
    for (int i = 0; i < max_threads; i++){
        if (pthread_create(&threads[i], NULL, thread_queue, 
                                             (void *) &pool_jobs)) {
            fprintf(stderr, "Failed to create thread\n");
            continue;
        }   
    }

    while (1){
        Message msg;
        // Ler pedido do proximo cliente (bloqueante)
        ssize_t bytes_read = read(fd_register, &msg, sizeof(msg));
        if (bytes_read > 0){
            if (msg.opcode == 1) { // Conexão de cliente
                char req_pipe_path[256];
                char resp_pipe_path[256];
                char notif_pipe_path[256];

                // Parse the message data to get the pipe paths
                sscanf(msg.data, "%s|%s|%s", req_pipe_path, resp_pipe_path, notif_pipe_path);

                // Enfileira o cliente na fila de clientes
                //identificador do cliente é o file descriptor
                int client_fd = open(req_pipe_path, O_RDONLY);
                if (client_fd != -1) {
                    enqueue_client(pool_clients, client_fd);
                }
            }
        }

        // Verifica e processa a fila de clientes
        if (!is_client_queue_empty(pool_clients)) {
            int client_fd = dequeue_client(pool_clients);
            if (client_fd != -1) {
                process_client(client_fd);
            }
        }

    }
    
    //espera que todos os backups terminem antes de terminar
    for (int i = 0; i < backup_limit; i++){
        wait(NULL);
    }

    close(fd_register);
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

    Queue pool_jobs;
    //Tira a pool de tarefas relacionadas com a diretoria
    iterates_files(argv[1], backup_limit, &pool_jobs);
    ClientQueue pool_clients;
    //Inicializa a pool de tarefas relacionadas com os clientes
    //tarefa anfitriã
    master_task(&pool_jobs, &pool_clients, server_fifo, max_threads, backup_limit, threads);

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


