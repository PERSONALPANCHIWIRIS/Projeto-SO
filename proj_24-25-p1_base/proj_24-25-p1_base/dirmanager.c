#include "dirmanager.h"


int count_files(const char *dir_path){
    int count = 0;
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(dir_path)) == NULL){
        fprintf(stderr, "Failed to open directory\n");
        closedir(dir);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL){
        if (strstr(entry->d_name, ".job") != NULL){
            count++;
        }
    }

    closedir(dir);
    return count;
}


void register_files(const char *dir_path, char files[][MAX_JOB_FILE_NAME_SIZE]){
    int count = 0;
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(dir_path)) == NULL){
        fprintf(stderr, "Failed to open directory\n");
        closedir(dir);
        return;
    }

    while ((entry = readdir(dir)) != NULL){
        if (strstr(entry->d_name, ".job") != NULL){
            int result = snprintf(files[count], MAX_JOB_FILE_NAME_SIZE, "%s/%s", dir_path, entry->d_name);
            if(result > MAX_JOB_FILE_NAME_SIZE){
                fprintf(stderr, "File name too long\n");
                continue;
            }
            count++;
        }
    }
    closedir(dir);
    return;
}

int manage_file(const char *file_path){
    int fd_in; int fd_out;
    char file_out[MAX_JOB_FILE_NAME_SIZE];

    fd_in = open(file_path, O_RDONLY);

    if (fd_in == -1){
        fprintf(stderr, "Failed to open file %s\n", file_path);
        close(fd_in);
        return 1;
    }

    strncpy(file_out, file_path, MAX_JOB_FILE_NAME_SIZE);
    char *extension = strstr(file_out, ".job");
    if (extension != NULL) {
        strcpy(extension, ".out");
    }

    fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_out == -1){
        fprintf(stderr, "Failed to create .out file %s\n", file_out);
        close(fd_in);
        return 1;
    }


  

  while (1) {
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    unsigned int delay;
    size_t num_pairs;

    //fflush(stdout);

    switch (get_next(fd_in)) {
      case CMD_WRITE:
        num_pairs = parse_write(fd_in, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_write(num_pairs, keys, values)) {
          fprintf(stderr, "Failed to write pair\n");
        }

        break;

      case CMD_READ:
        num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(num_pairs, keys, fd_out)) {
          fprintf(stderr, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

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
          printf("Waiting...\n");
          kvs_wait(delay);
        }
        break;

      case CMD_BACKUP:

        if (kvs_backup()) {
          fprintf(stderr, "Failed to perform backup.\n");
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        printf( 
            "Available commands:\n"
            "  WRITE [(key,value),(key2,value2),...]\n"
            "  READ [key,key2,...]\n"
            "  DELETE [key,key2,...]\n"
            "  SHOW\n"
            "  WAIT <delay_ms>\n"
            "  BACKUP\n" // Not implemented
            "  HELP\n"
        );

        break;
        
      case CMD_EMPTY:
        break;

      case EOC:
        kvs_terminate();
        return 0;
    }
  }
  close(fd_in);
  close(fd_out);
  return 0;
}
