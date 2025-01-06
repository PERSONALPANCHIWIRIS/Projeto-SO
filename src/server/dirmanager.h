#ifndef DIRMANAGER_H
#define DIRMANAGER_H

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "parser.h"
#include "operations.h"

extern pthread_mutex_t global_lock;

typedef struct Node {
    char file_path[MAX_JOB_FILE_NAME_SIZE];
    struct Node* next;
} Node;

typedef struct Queue {
    Node *first, *last;
    int backup_limit;
} Queue;

void iterates_files(const char *dir_path, int backup_limit, Queue* q);
void manage_file(char *file_path, int backup_limit);
void *thread_queue(void *arg);
void init_queue(Queue* q);
int is_empty(Queue* q);
void enqueue(Queue* q, const char* file_path);
char* dequeue(Queue* q);

#endif // DIRMANAGER_H