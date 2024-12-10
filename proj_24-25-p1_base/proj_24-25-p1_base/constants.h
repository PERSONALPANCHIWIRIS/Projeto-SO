#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MAX_WRITE_SIZE 256
#define MAX_STRING_SIZE 40
#define MAX_JOB_FILE_NAME_SIZE 256

struct file_info {
    char file_path[MAX_JOB_FILE_NAME_SIZE];
    int backup_limit;
};


#endif //CONSTANTS_H