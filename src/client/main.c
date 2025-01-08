#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/constants.h"
#include "src/common/io.h"

bool stop_notifications = false;

void read_notifications(void* arg) {
  int *notif_pipe = (int *) arg;
  char buffer[256];
  //Ciclo infinito sempre à espera de notificações
  while (!stop_notifications){
    ssize_t bytes_read = read(*notif_pipe, buffer, sizeof(buffer));
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      fprintf(stdout, "%s\n", buffer);
    }
  }
  close(*notif_pipe);
  return; 
}


int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notif_pipe_path[256] = "/tmp/notif";

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  //Caso existirem antes de serem criados
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

  // Criação dos named pipes
  if (mkfifo(req_pipe_path, 0666) == -1) {
    perror("Failed to create request FIFO");
    return 1;
  }

  if (mkfifo(resp_pipe_path, 0666) == -1) {
    perror("Failed to create response FIFO");
    return 1;
  }

  if (mkfifo(notif_pipe_path, 0666) == -1) {
    perror("Failed to create notification FIFO");
    return 1;
  }

  // Abrir o FIFO de notificações
  int notif_pipe = open(notif_pipe_path, O_RDONLY | O_NONBLOCK);
  if (notif_pipe == -1) {
    perror("Error opening notification FIFO");
    return 1;
  }

  // Thread para ler as notificações
  pthread_t notif_thread;
  pthread_create(&notif_thread, NULL, (void*) read_notifications, (void*) &notif_pipe);

  // TODO open pipes
  if (kvs_connect(req_pipe_path, resp_pipe_path, argv[2], notif_pipe_path, &notif_pipe) != 0) {
    fprintf(stderr, "Failed to connect to the server\n");
    return 1;
  }

  while (1) {
    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(req_pipe_path, resp_pipe_path) != 0) {
          fprintf(stderr, "Failed to disconnect to the server\n");
          return 1;
        }
        // TODO: end notifications thread
        stop_notifications = true;
        pthread_join(notif_thread, NULL);
        //Apaga as pipes criadas
        if (unlink(req_pipe_path) == -1) {
        perror("Failed to remove request FIFO");
        }
        if (unlink(resp_pipe_path) == -1) {
            perror("Failed to remove response FIFO");
        }
        if (unlink(notif_pipe_path) == -1) {
            perror("Failed to remove notification FIFO");
        }
        printf("Disconnected from server\n");
        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
         
        if (kvs_subscribe(keys[0], req_pipe_path, resp_pipe_path)) {
            fprintf(stderr, "Command subscribe failed\n");
        }

        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
         
        if (kvs_unsubscribe(keys[0], req_pipe_path, resp_pipe_path)) {
            fprintf(stderr, "Command subscribe failed\n");
        }

        break;

      case CMD_DELAY:
        if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay_ms > 0) {
            printf("Waiting...\n");
            delay(delay_ms);
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        // input should end in a disconnect, or it will loop here forever
        break;
    }
  }
}
