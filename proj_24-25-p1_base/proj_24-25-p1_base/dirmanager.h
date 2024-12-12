#ifndef DIRMANAGER_H
#define DIRMANAGER_H


#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

//preciso destes?? posso usar se quer??
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
//Penso que não

#include "parser.h"
#include "operations.h"
#include "constants.h"


typedef struct Node {
    char file_path[MAX_JOB_FILE_NAME_SIZE];
    struct Node* next;
} Node;

typedef struct Queue {
    Node* first;
    Node* last;
    int backup_limit;
} Queue;

// int count_files(const char *dir_path);
// void register_files(const char *dir_path, char files[][MAX_JOB_FILE_NAME_SIZE]);
void iterates_files(const char *dir_path, int backup_limit, int max_threads, pthread_t *threads);
void manage_file(char *file_path, int backup_limit);
void *thread_queue(void *arg);
void init_queue(Queue* q);
int is_empty(Queue* q);
void enqueue(Queue* q, const char* file_path);
char* dequeue(Queue* q);


extern pthread_mutex_t global_lock;

#endif // DIRMANAGER_H