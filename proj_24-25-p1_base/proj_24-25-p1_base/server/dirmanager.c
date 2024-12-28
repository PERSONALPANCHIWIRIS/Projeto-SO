#include <pthread.h>
#include <stdbool.h>

#include "dirmanager.h"
#include "operations.h"

//lock para variaveis globais
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
//lock para a queue
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
bool done_reading = false;
Queue q;


//------------------------------------------------------------ CODE:
//inicializa a queue
void init_queue(Queue* queue) {
    queue->first = queue->last = NULL;
}


//verifica se a queue está vazia
int is_empty(Queue* queue) {
    return queue->first == NULL;
}


//adiciona um ficheiro à queue
void enqueue(Queue* queue, const char* file_path) {
    Node *temp = (Node *)malloc(sizeof(Node));

    strncpy(temp->file_path, file_path, MAX_JOB_FILE_NAME_SIZE);
    temp->file_path[MAX_JOB_FILE_NAME_SIZE - 1] = '\0';
    temp->next = NULL;

    //caso em que é o primeiro ficheiro na queue
    if (queue->last == NULL) {
        queue->first = queue->last = temp;
        return;
    }

    //caso a queue não esteja vazia, o ficheiro é adicionado no fim
    queue->last->next = temp;
    queue->last = temp;
}


//remove um ficehiro da queue (sempre o primeiro)
char* dequeue(Queue* queue) {
    //condiçáo de paragem
    if (is_empty(queue)) {
        return NULL;
    }

    //ponteiro temporário que vai auxiliar na remoção
    Node* temp = queue->first;
    char* file_path = strdup(temp->file_path);

    //O primeiro da fila passa a ser o seguinte
    queue->first = queue->first->next;

    if (queue->first == NULL) {
        queue->last = NULL;
    }
    
    free(temp);
    temp = NULL;

    return file_path;
}


//entramos na diretoria e iteramos pelos ficheiros
void iterates_files(const char *dir_path, int backup_limit, 
                    int max_threads,pthread_t *threads) {
    DIR *dir;
    struct dirent *entry;
    char filepath[MAX_JOB_FILE_NAME_SIZE];

    //inicializamos a queue
    pthread_mutex_lock(&queue_lock);
    init_queue(&q);
    q.backup_limit = backup_limit;
    pthread_mutex_unlock(&queue_lock);

    //abrimos a diretoria
    if ((dir = opendir(dir_path)) == NULL){
        fprintf(stderr, "Failed to open directory\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL){
        //já dentro da diretoria, acedemos apenas aos ficheiros .job
        if (strstr(entry->d_name, ".job")){
            //Verificação do comprimento do nome
            if ((strlen(dir_path) + strlen(entry->d_name) + 1)
                                     > MAX_JOB_FILE_NAME_SIZE){
                fprintf(stderr, "File name too long\n");
                break;
            }
            
            //Constroi o nome do ficheiro (filepath): diretoria/nome_ficheiro
            strcpy(filepath, dir_path);
            strcat(filepath, "/");
            strcat(filepath, entry->d_name);

            //adiciona o ficheiro à queue
            pthread_mutex_lock(&queue_lock);
            enqueue(&q, filepath);
            pthread_mutex_unlock(&queue_lock);
        }    
    }

    //já foi lida a diretoria inteira
    pthread_mutex_lock(&queue_lock);
    done_reading = true;
    pthread_mutex_unlock(&queue_lock);

    //inicializa as threads para a função thread_queue
    for (int i = 0; i < max_threads; i++){
        if (pthread_create(&threads[i], NULL, thread_queue, 
                                             (void *) &q)) {
            fprintf(stderr, "Failed to create thread\n");
            continue;
        }   
    }
    
    //espera que todos os backups terminem antes de fechar a diretoria
    for (int i = 0; i < backup_limit; i++){
        wait(NULL);
    }

    closedir(dir);

    return;
}


/**
 * função executada pelas threads, que processa os ficheiros até a 
 * queue estar vazia. Desta forma cada thread está num loop e processa 
 * um ficheiro, sempre que possível.
 */
void *thread_queue(void *arg) {
    Queue *local_q = (Queue *)arg;

    //loop infinito que irá parar quando a queue estiver vazia
    while (1) {
        pthread_mutex_lock(&queue_lock);

        //caso em que a queue está vazia e á lemos todos os ficheiros
        if (is_empty(local_q) && done_reading) {
            pthread_mutex_unlock(&queue_lock);
            return NULL;
        }

        //retira um ficheiro da queue
        char* file_path = dequeue(local_q);
        pthread_mutex_unlock(&queue_lock);

        if (file_path == NULL) {
            return NULL;
        }

        //chama a função de processamento do ficheiro
        manage_file(file_path, local_q->backup_limit);
    }
}


//processa os comandos do ficheiro e cria o ficheiro .out correspondente
void manage_file(char *file_path, int backup_limit) {
    int fd_in, fd_out;
    char file_out[MAX_JOB_FILE_NAME_SIZE];
    int backup_count = 0;

    //abrimos o ficheiro de input
    fd_in = open(file_path, O_RDONLY);

    if (fd_in == -1){
        fprintf(stderr, "Failed to open input file %s\n", file_path);
        return;
    }

    //copiamos o nome do ficheiro de input, alterando apenas a extensão
    strncpy(file_out, file_path, MAX_JOB_FILE_NAME_SIZE-1);
            file_out[MAX_JOB_FILE_NAME_SIZE-1] = '\0';
    char *extension = strstr(file_out, ".job");
    if (extension) {
        strcpy(extension, ".out");
    }

    //criamos o ficheiro de output
    fd_out = open(file_out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd_out == -1){
        fprintf(stderr, "Failed to create .out file\n");
        close(fd_in);
        return;
    }

    //processa os comandos no ficheiro de input
    while (1) {
        char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
        char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
        unsigned int delay;
        size_t num_pairs;

        switch (get_next(fd_in)) {
            case CMD_WRITE:
                num_pairs = parse_write(fd_in, keys, values,
                                        MAX_WRITE_SIZE, MAX_STRING_SIZE);
                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (kvs_write(num_pairs, keys, values)) {
                    fprintf(stderr, "Failed to write pair\n");
                }
                break;

            case CMD_READ:
                num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, 
                                              MAX_STRING_SIZE);

                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (kvs_read(num_pairs, keys, fd_out)) {
                    fprintf(stderr, "Failed to read pair\n");
                }
                break;

            case CMD_DELETE:
                num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE,
                                              MAX_STRING_SIZE);

                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (kvs_delete(num_pairs, keys, fd_out)) {
                    fprintf(stderr, "Failed to delete pair\n");
                }
                break;

            case CMD_SHOW:
                kvs_show(fd_out);
                break;

            case CMD_WAIT:
                if (parse_wait(fd_in, &delay, NULL) == -1) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (delay > 0) {
                    write(fd_out, "Waiting...\n", 11);
                    kvs_wait(delay);
                }
                break;

            case CMD_BACKUP:  
                //É sempre verificado se o número de backups atingiu o limite
                pthread_mutex_lock(&global_lock);
                if (current_backup >= backup_limit){
                    pthread_mutex_unlock(&global_lock);
                    wait(NULL);     //espera pelo fim de um backup
                    pthread_mutex_lock(&global_lock);
                    current_backup--;
                }
                pthread_mutex_unlock(&global_lock);

                //Chama a função de backup
                if (kvs_backup(backup_count, file_path)) {
                    fprintf(stderr, "Failed to perform backup.\n");
                }

                pthread_mutex_lock(&global_lock);
                current_backup++;
                pthread_mutex_unlock(&global_lock);
                backup_count++;     //muda na função "global"

                break;

            case CMD_INVALID:
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                break;

            case CMD_HELP:
                write(fd_out, 
                    "Available commands:\n"
                    "  WRITE [(key,value)(key2,value2),...]\n"
                    "  READ [key,key2,...]\n"
                    "  DELETE [key,key2,...]\n"
                    "  SHOW\n"
                    "  WAIT <delay_ms>\n"
                    "  BACKUP\n"
                    "  HELP\n", 
                    136);
                break;

            case CMD_EMPTY:
                break;

            case EOC:
                free(file_path);
                file_path = NULL;
                close(fd_in);
                close(fd_out);
                return;
        }
    }
    
    return;
}
