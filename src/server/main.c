#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "dirmanager.h"
#include "../client/api.h"
#include "../common/io.h"
#include "subscription.h"

int current_backup = 0;
int current_threads = 0;
Queue q;

pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;
sem_t client_sem;

typedef struct ClientNode {
    char req_pipe_path[256];      // Caminho do pipe de pedidos
    char resp_pipe_path[256];     // Caminho do pipe de resposta
    char notif_pipe_path[256];    // Caminho do pipe de notificação
    struct ClientNode* next;      // Próximo cliente na fila
} ClientNode;

typedef struct ClientQueue {
    ClientNode* front;            // Início da fila
    ClientNode* rear;             // Fim da fila
    pthread_mutex_t mutex;        // Mutex para sincronização
    pthread_cond_t cond;          // Condição para notificar threads
} ClientQueue;

SubscriptionMap* subscription_map;


// Inicializar a fila de clientes
void init_client_queue(ClientQueue* queue) {
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Enfileirar um cliente na fila
void enqueue_client(ClientQueue* queue, const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path) {
    ClientNode* new_node = (ClientNode*)malloc(sizeof(ClientNode));
    strncpy(new_node->req_pipe_path, req_pipe_path, sizeof(new_node->req_pipe_path) - 1);
    strncpy(new_node->resp_pipe_path, resp_pipe_path, sizeof(new_node->resp_pipe_path) - 1);
    strncpy(new_node->notif_pipe_path, notif_pipe_path, sizeof(new_node->notif_pipe_path) - 1);
    new_node->req_pipe_path[sizeof(new_node->req_pipe_path) - 1] = '\0';
    new_node->resp_pipe_path[sizeof(new_node->resp_pipe_path) - 1] = '\0';
    new_node->notif_pipe_path[sizeof(new_node->notif_pipe_path) - 1] = '\0';
    new_node->next = NULL;

    pthread_mutex_lock(&queue->mutex);
    if (queue->rear == NULL) {
        //Primeiro cliente
        queue->front = queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }
    pthread_cond_signal(&queue->cond); // Notificar threads esperando na fila
    pthread_mutex_unlock(&queue->mutex);

    //Signal para o semaforo
    sem_post(&client_sem);
}

// Desenfileirar um cliente da fila
ClientNode* dequeue_client(ClientQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->front == NULL) {
        //Isto é, para uma queue vazia, espera 
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    ClientNode* temp = queue->front;
    //int client_fd = temp->client_fd;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        //Se a fila ficar vazia
        queue->rear = NULL;
    }
    //free(temp);
    pthread_mutex_unlock(&queue->mutex);
    return temp;
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

//OPERACOES COM CLIENTES--------------------------------------------------------------------------------------------------------------
bool process_client_request(Message* msg, const char* resp_pipe_path, const char* notif_pipe_path,
 int client_notif_fd, int client_req_fd) {
    int client_resp_fd = open(resp_pipe_path, O_WRONLY);
    switch (msg->opcode) {       
        case 2:
            // Processar desconexão
            //Envia a mensagem de desconexão
            if (client_resp_fd != -1) {
                //Sucesso
                char response[2] = {2, 0};
                write_all(client_resp_fd, response, 2);
                //write_all(client_resp_fd, "Server returned 0 for operation: 2\n", 36);    
            }
            else{
                //Erro
                char response[2] = {2, 1};
                write_all(client_resp_fd, response, 2);
                //write_all(client_resp_fd, "Server returned 1 for operation: 2\n", 36);
            }
            close(client_resp_fd);

            remove_all_subscriptions(subscription_map, notif_pipe_path);
            close(client_notif_fd); //Fecha o de notificações
            close(client_req_fd); //Fecha o de pedidos
            return true;

        case 3:
            // Processar subscrição
            //Envia a mensagem de desconexão
            if (client_resp_fd == -1) {
                char response_sub[2] = {3, 1};
                write_all(client_resp_fd, response_sub, 2);
                //write_all(client_resp_fd, "Server returned 1 for operation: 3\n", 36);    
            }
            
            int existed = add_subscription(subscription_map, msg->key, notif_pipe_path);
            if (existed == 1){
                char response_sub[2] = {3, 1};
                write_all(client_resp_fd, response_sub, 2);
                //write_all(client_resp_fd, "Server returned 1 for operation: 3\n", 36);
            }
            else{
                char response_sub[2] = {3, 0};
                write_all(client_resp_fd, response_sub, 2);
                //write_all(client_resp_fd, "Server returned 0 for operation: 3\n", 36);
            }
            close(client_resp_fd);
            return false;

        case 4:
            // Processar cancelamento de subscrição
            //Envia a mensagem de desconexão
            if (client_resp_fd == -1) {//Erro
                char response_unsub[2] = {4, 1};
                write_all(client_resp_fd, response_unsub, 2);
                //write_all(client_resp_fd, "Server returned 1 for operation: 4\n", 36);    
            }

            int existed_unsub = remove_subscription(subscription_map, msg->key, notif_pipe_path);
            if (existed_unsub == 1){ //Não existia
                char response_unsub[2] = {4, 1};
                write_all(client_resp_fd, response_unsub, 2);
                //write_all(client_resp_fd, "Server returned 1 for operation: 4\n", 36);
            }
            else{ //Existia
                char response_unsub[2] = {4, 0};
                write_all(client_resp_fd, response_unsub, 2);
                //write_all(client_resp_fd, "Server returned 0 for operation: 4\n", 36);
            }
            close(client_resp_fd);
            return false;

        default:
            return false;
    }
}

//Le a mensagem do cliente e processa-a
void process_client(const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path) {
    bool done = false;
    int client_notif_fd = open(notif_pipe_path, O_WRONLY);//Abre o pipe de notificações para este cliente
    while(!done){
        Message msg;
        // Ler pedido do cliente (bloqueante)
        int client_req_fd = open(req_pipe_path, O_RDONLY);
        ssize_t bytes_read = read_all(client_req_fd, &msg, sizeof(msg), NULL);
        if (bytes_read <= 0) {
            perror("Error reading request from client");
            break;
        }

        // Processar o pedido do cliente
        done = process_client_request(&msg, resp_pipe_path, notif_pipe_path, client_notif_fd, client_req_fd);
    }
}

void *thread_client(void *arg) {
    ClientQueue* pool_clients = (ClientQueue*) arg;
    while (1) {
        sem_wait(&client_sem);

        pthread_mutex_lock(&client_lock);
         while (is_client_queue_empty(pool_clients)) {
            pthread_cond_wait(&client_cond, &client_lock);
        }

        ClientNode* temp = dequeue_client(pool_clients);
        if (temp == NULL) {
            pthread_mutex_unlock(&client_lock);
            continue;
        }
        pthread_mutex_unlock(&client_lock);
        process_client(temp->req_pipe_path, temp->resp_pipe_path, temp->notif_pipe_path);
        free(temp);
    }
}

void master_task(ClientQueue* pool_clients, const char* server_fifo,
 int max_threads, int backup_limit, pthread_t *threads, DIR* dir) {
    pthread_t client_threads[S];

    ThreadQueueArgs thread_args = {&q, dir, backup_limit};

    for (int i = 0; i < max_threads; i++){
        if (pthread_create(&threads[i], NULL, thread_queue, 
                                                (void *) &thread_args)) {
            fprintf(stderr, "Failed to create thread\n");
            continue;
        }   
    }

    for (int i = 0; i < S; i++){
        if (pthread_create(&client_threads[i], NULL, thread_client, 
                                             (void *) pool_clients)) {
            fprintf(stderr, "Failed to create thread\n");
            continue;
        }   
    }

    //Trata dos jobs relacionados com a diretoria (com threads)
    //iterates_files(dir_path, backup_limit, max_threads, threads);

    int fd_register = open(server_fifo, O_RDONLY); //Abre a de registo do lado do server
    if (fd_register == -1) {
        perror("Error opening FIFO");
        return;
    }
    while (1){  //Loop infinito a espera de clientes
        Message msg;
        //Ler pedido do proximo cliente (bloqueante)
        //Le do fifo de registo a mensagem de connect
        ssize_t bytes_read = read_all(fd_register, &msg, sizeof(msg), NULL);
        if (bytes_read > 0){
            if (msg.opcode == 1) { // Conexão de cliente
                char req_pipe_path[40];
                char resp_pipe_path[40];
                char notif_pipe_path[40];

                // Tira o caminho das pipes do cliente
                //sscanf(msg.data, " %255[^|]| %255[^|]| %255[^|]", req_pipe_path, resp_pipe_path, notif_pipe_path);
                sscanf(msg.data, "%40s %40s %40s", req_pipe_path, resp_pipe_path, notif_pipe_path);

                // Enfileira o cliente na fila de clientes
                enqueue_client(pool_clients, req_pipe_path, resp_pipe_path, notif_pipe_path);
                int client_resp_fd = open(resp_pipe_path, O_WRONLY);
                if (client_resp_fd != -1) {
                    //Isto para a operação connect
                    char response[2] = {'1', '0'};
                    write_all(client_resp_fd, response, 2);
                    //write_all(client_resp_fd, "Server returned 0 for operation: 1\n", 34);
                }
                else{
                    char response[2] = {'1', '1'};
                    write_all(client_resp_fd, response, 2);
                    //write_all(client_resp_fd, "Server returned 1 for operation: 1\n", 34);
                }
                // Sinaliza que já podem ser processados clientes
                pthread_cond_broadcast(&client_cond);
                close(client_resp_fd);
            }
        }
        // Inicializa a estrutura de argumentos para as threads
        // ThreadQueueArgs thread_args = {&q, dir, backup_limit};

        //inicializa as threads para a função thread_queue
        // for (int i = 0; i < max_threads; i++){
        //     if (pthread_create(&threads[i], NULL, thread_queue, 
        //                                          (void *) &thread_args)) {
        //         fprintf(stderr, "Failed to create thread\n");
        //         continue;
        //     }   
        // }

        // Verifica e processa a fila de clientes
        // if (!is_client_queue_empty(pool_clients)) {
        //     ClientNode* temp = dequeue_client(pool_clients);
        //     process_client(temp->req_pipe_path, temp->resp_pipe_path, temp->notif_pipe_path);
        //     free(temp);
        // }

    }
    
    // //espera que todos os backups terminem antes de terminar
    // for (int i = 0; i < backup_limit; i++){
    //     wait(NULL);
    // }

    //close(fd_register);
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

    //cria o mapa de subscrições
    subscription_map = create_subscription_map();
    if (!subscription_map) {
        fprintf(stderr, "Failed to create subscription map\n");
        return 1;
    }

    // Remove FIFO de registo se existir
    unlink(argv[4]);

    const char *server_fifo = argv[4];
    if (mkfifo(server_fifo, 0666) == -1) {
        perror("Failed to create FIFO");
        return 1;
    }

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        return 1;
    }

    const char* dir_path = argv[1];
    //Tira a pool de tarefas relacionadas com a diretoria
    ClientQueue pool_clients; //Inicializa a pool de tarefas relacionadas com os clientes
    //inicializa a pool de tarefas dos jobs
    DIR *dir = iterates_files(dir_path, backup_limit);

    //inicializa o semaforo
    sem_init(&client_sem, 0, 0);

    //tarefa anfitriã
    master_task(&pool_clients, server_fifo, max_threads, backup_limit, threads, dir);

    //esperamos que todas as threads terminem
    for (int i = 0; i < max_threads; i++) {
        if (threads[i] != 0) {
            pthread_join(threads[i], NULL);
        }
    }

    sem_destroy(&client_sem);
    pthread_mutex_destroy(&client_lock);
    pthread_cond_destroy(&client_cond);
    //libertamos a memoria alocada da hash table
    kvs_terminate();
    free_subscription_map(subscription_map);

    return 0;
}


