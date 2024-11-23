#include "dirmanager.h"

/*      MENSAGENS DE ERRO VÃO PARA O STDERR     */


//Opens directory and iterates through the files
void iterates_files(const char *dir_path) {
    DIR *dir;
    struct dirent *entry;
    char filepath[MAX_JOB_FILE_NAME_SIZE];

    if ((dir = opendir(dir_path)) == NULL){
        //write(STDERR_FILENO, "Failed to open directory\n", 25);
        fprintf(stderr, "Failed to open directory\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL){
        if (strstr(entry->d_name, ".job") != NULL){

            if ((strlen(dir_path) + strlen(entry->d_name) + 1) > MAX_JOB_FILE_NAME_SIZE){
               //write(STDERR_FILENO, "File name too long\n", 20);
               fprintf(stderr, "File name too long\n");
               continue;
            }

            strcpy(filepath, dir_path);
            strcat(filepath, "/"); //concatenar
            strcat(filepath, entry->d_name);

            manage_file(filepath);
        }
    }

    closedir(dir);
    return;
}


//Processes each command in the file and creates the corresponding .out file
int manage_file(const char *file_path){
    int fd_in; int fd_out;
    char file_out[MAX_JOB_FILE_NAME_SIZE];

    if (kvs_init()) {
        //write(STDERR_FILENO, "Failed to initialize KVS\n", 26);
        fprintf(stderr, "Failed to initialize KVS\n");

        return 1;
    }

    fd_in = open(file_path, O_RDONLY);

    if (fd_in == -1){
        //write(STDERR_FILENO, "Failed to open input file %s\n", 26);
        fprintf(stderr, "Failed to open input file %s\n", file_path);

        //close(fd_in);
        return 1;
    }

    strncpy(file_out, file_path, MAX_JOB_FILE_NAME_SIZE-1);
    file_out[MAX_JOB_FILE_NAME_SIZE-1] = '\0';
    char *extension = strstr(file_out, ".job");
    if (extension != NULL) {
        strcpy(extension, ".out");
    }

    fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd_out == -1){
        //write(STDERR_FILENO, "Failed to create .out file %s\n", 29);
        //fprintf(stderr, "Failed to create .out file %s\n", );
        fprintf(stderr, "Failed to create .out file\n");

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
                //write(fd_out, "Invalid command. See HELP for usage\n", 36);
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
            }

            if (kvs_write(num_pairs, keys, values)) {
                //write(fd_out, "Failed to write pair\n", 22);
                fprintf(stderr, "Failed to write pair\n");
            }
            break;

        case CMD_READ:
            num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

            if (num_pairs == 0) {
                //write(fd_out, "Invalid command. See HELP for usage\n", 36);
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
            }

            if (kvs_read(num_pairs, keys, fd_out)) {
                //write(fd_out, "Failed to read pair\n", 21);
                fprintf(stderr, "Failed to read pair\n");
            }
            break;

        case CMD_DELETE:
            num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

            if (num_pairs == 0) {
                //write(fd_out, "Invalid command. See HELP for usage\n", 36);
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
            }

            if (kvs_delete(num_pairs, keys, fd_out)) {
                //write(fd_out, "Failed to delete pair\n", 23);
                fprintf(stderr, "Failed to delete pair\n");
            }
            break;

        case CMD_SHOW:
            kvs_show(fd_out);
            break;

        case CMD_WAIT:
            if (parse_wait(fd_in, &delay, NULL) == -1) {
                //write(fd_out, "Invalid command. See HELP for usage\n", 36);
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
            }

            if (delay > 0) {
                write(fd_out, "Waiting...\n", 11);
                kvs_wait(delay);
            }
            break;

        case CMD_BACKUP:
            if (kvs_backup()) {
                //write(fd_out, "Failed to perform backup.\n", 26);
                fprintf(stderr, "Failed to perform backup.\n");
            }
            break;

        case CMD_INVALID:
            //write(fd_out, "Invalid command. See HELP for usage\n", 36);
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
                "  BACKUP\n" // Not implemented
                "  HELP\n", 
                136);
            break;

        case CMD_EMPTY:
            break;

        case EOC:
            kvs_terminate();
            close(fd_in);
            close(fd_out);
            return 0;
    }
  }
}





//     switch (get_next(fd_in)) {
//       case CMD_WRITE:
//         num_pairs = parse_write(fd_in, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
//         if (num_pairs == 0) {
//           fprintf(stderr, "Invalid command. See HELP for usage\n");
//           continue;
//         }

//         if (kvs_write(num_pairs, keys, values)) {
//           fprintf(stderr, "Failed to write pair\n");
//         }

//         break;

//       case CMD_READ:
//         num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

//         if (num_pairs == 0) {
//           fprintf(stderr, "Invalid command. See HELP for usage\n");
//           continue;
//         }

//         if (kvs_read(num_pairs, keys, fd_out)) {
//           fprintf(stderr, "Failed to read pair\n");
//         }
//         break;

//       case CMD_DELETE:
//         num_pairs = parse_read_delete(fd_in, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

//         if (num_pairs == 0) {
//           fprintf(stderr, "Invalid command. See HELP for usage\n");
//           continue;
//         }

//         if (kvs_delete(num_pairs, keys, fd_out)) {
//           fprintf(stderr, "Failed to delete pair\n");
//         }
//         break;

//       case CMD_SHOW:

//         kvs_show(fd_out);
//         break;

//       case CMD_WAIT:
//         if (parse_wait(fd_in, &delay, NULL) == -1) {
//           fprintf(stderr, "Invalid command. See HELP for usage\n");
//           continue;
//         }

//         if (delay > 0) {
//           printf("Waiting...\n");
//           kvs_wait(delay);
//         }
//         break;

//       case CMD_BACKUP:

//         if (kvs_backup()) {
//           fprintf(stderr, "Failed to perform backup.\n");
//         }
//         break;

//       case CMD_INVALID:
//         fprintf(stderr, "Invalid command. See HELP for usage\n");
//         break;

//       case CMD_HELP:
//         printf( 
//             "Available commands:\n"
//             "  WRITE [(key,value),(key2,value2),...]\n"
//             "  READ [key,key2,...]\n"
//             "  DELETE [key,key2,...]\n"
//             "  SHOW\n"
//             "  WAIT <delay_ms>\n"
//             "  BACKUP\n" // Not implemented
//             "  HELP\n"
//         );

//         break;
        
//       case CMD_EMPTY:
//         break;

//       case EOC:
//         kvs_terminate();
//         return 0;
//     }
//   }
//   close(fd_in);
//   close(fd_out);
//   return 0;
// }
