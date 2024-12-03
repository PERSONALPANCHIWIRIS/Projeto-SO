#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "kvs.h"
#include "constants.h"
#include "operations.h"

static struct HashTable* kvs_table = NULL;
/*      O QUE ESTÁ NO STDERR FICA NO STDERR, O RESTO VAI PARA O FICHEIRO    */


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init() {
  if (kvs_table != NULL) {
    fprintf(stderr, "KVS state has already been initialized\n");
    return 1;
  }

  kvs_table = create_hash_table();
  return kvs_table == NULL;
}

int kvs_terminate() {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  free_table(kvs_table);
  kvs_table = NULL;
  return 0;
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }

  return 0;
}

/*
//READ ANTIGO
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  dprintf(fd_out, "[");
  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      dprintf(fd_out, "(%s,KVSERROR)", keys[i]);
    } else {
      dprintf(fd_out, "(%s,%s)", keys[i], result);
    }
    //debug para memory leaks
    free(result);
  }
  dprintf(fd_out, "]\n");
  return 0;
}
*/

//VERSÃO NOVA DO CODIGO BASE 
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  //printf("[");
  write(fd_out, "[", 1);
  /*
  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      printf("(%s,KVSERROR)", keys[i]);
    } else {
      printf("(%s,%s)", keys[i], result);
    }
    free(result);
  }
  */
    for (size_t i = 0; i < num_pairs; i++) {
        char *result = read_pair(kvs_table, keys[i]);
        write(fd_out, "(", 1);
        write(fd_out, keys[i], strlen(keys[i]));
        if (result == NULL) {
            write(fd_out, ",KVSERROR", 9);
        } else {
            write(fd_out, ",", 1);
            write(fd_out, result, strlen(result));
        }
        write(fd_out, ")", 1);
        free(result);
    }

  //printf("]\n");
  write(fd_out, "]\n", 2);
  return 0;
}


//VERSÃO DO NOVO CODIGO BASE
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }
  int aux = 0;

  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0) {
      if (!aux) {
        write(fd_out, "[", 1);
        aux = 1;
      }
      //printf("(%s,KVSMISSING)", keys[i]);
        write(fd_out, "(", 1);
        write(fd_out, keys[i], strlen(keys[i]));
        write(fd_out, ",KVSMISSING)", 12);
    }
  }
  if (aux) {
    write(fd_out, "]\n", 2);
  }

  return 0;
}


/*
-------------------- NOSSA VERSÃO--------------------
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
    int count_error = 0;
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }
    // alterei para colocar os parentesis retos e o newline, que é o formato que vem no enunciado
  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0){
        if(count_error == 0) {
            dprintf(fd_out, "[(%s,KVSMISSING)", keys[i]);
        }
        else {
            dprintf(fd_out, "(%s,KVSMISSING)", keys[i]);
        }
        count_error++;
    }
  }

  if(count_error != 0) {
    dprintf(fd_out, "]\n");
  }

  return 0;
}
*/


void kvs_show(int fd_out) {
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = kvs_table->table[i];
    while (keyNode != NULL) {
        write(fd_out, "(", 1);
        write(fd_out, keyNode->key, strlen(keyNode->key));
        write(fd_out, ",", 1);
        write(fd_out, keyNode->value, strlen(keyNode->value));
        write(fd_out, ")\n", 2);

      //dprintf(fd_out, "(%s, %s)\n", keyNode->key, keyNode->value);
      keyNode = keyNode->next; // Move to the next node
    }
  }
}

//current_backup é definida como extern (global para todos os ficheiros) no header de operations
int kvs_backup(int backup_count, int backup_limit, const char *file_path) {
    

    pid_t pid = fork();//Cria o fork e continua a executar o pai e o filho
    
    if (pid == 0) { // Processo filho
        char backup_file[MAX_JOB_FILE_NAME_SIZE];
        strncpy(backup_file, file_path, MAX_JOB_FILE_NAME_SIZE - 5);
        backup_file[MAX_JOB_FILE_NAME_SIZE - 5] = '\0';
        char *extension = strstr(backup_file, ".job");
        if (extension != NULL) {
          strcpy(extension, "\0");
        }
        char bck_num[10];
        sprintf(bck_num, "-%d", backup_count);
        strcat(backup_file, bck_num);
        strcat(backup_file, ".bck"); //Isto cria o nome do ficheiro de backup
        int fd_backup = open(backup_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        //cria o ficheiro de backup
        //kvs_show(fd_backup); //neste caso não sei o que vai no ficheiro de backup

        for (int i = 0; i < TABLE_SIZE; i++) {
          KeyNode *keyNode = kvs_table->table[i];
          while (keyNode != NULL) {
              write(fd_backup, "(", 1);
              write(fd_backup, keyNode->key, strlen(keyNode->key));
              write(fd_backup, ",", 1);
              write(fd_backup, keyNode->value, strlen(keyNode->value));
              write(fd_backup, ")\n", 2);

            //dprintf(fd_out, "(%s, %s)\n", keyNode->key, keyNode->value);
            keyNode = keyNode->next; // Move to the next node
          }
        }


        close(fd_backup);
        exit(0);//Chamada para fechar o processo filho
      } 

      else if (pid > 0) { // Processo pai
        return 0; //nao tenho a certeza//a ideia é voltar ao dirmanager
      }
      else { 
        fprintf(stderr, "Failed to fork for backup\n");
      }
      return 0;
  }
  
  
  


void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}