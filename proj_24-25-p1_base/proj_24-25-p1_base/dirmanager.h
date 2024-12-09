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

#include "parser.h"
#include "operations.h"
#include "constants.h"

// int count_files(const char *dir_path);
// void register_files(const char *dir_path, char files[][MAX_JOB_FILE_NAME_SIZE]);
void iterates_files(const char *dir_path, int backup_limit, int max_threads);
void *manage_file(void*arg);	
extern pthread_mutex_t global_lock;

#endif // DIRMANAGER_H