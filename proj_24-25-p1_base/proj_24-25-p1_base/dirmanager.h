#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "parser.h"
#include "operations.h"
#include "constants.h"

int count_files(const char *dir_path);
void register_files(const char *dir_path, char files[][MAX_JOB_FILE_NAME_SIZE]);
int manage_file(const char *file_path);