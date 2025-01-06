#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int server_fd;

int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe) {

  // Abrir o FIFO do servidor
  server_fd = open(server_pipe_path, O_WRONLY);
  if (server_fd == -1) {
    perror("Error opening server FIFO");
    return 1;
  }
  notif_pipe++; //DEpois vemos o que fazemos com isto

  // Enviar mensagem de conexão para o servidor
  Message msg;
  msg.opcode = 1;
  snprintf(msg.data, sizeof(msg.data), "%s|%s|%s", req_pipe_path, resp_pipe_path, notif_pipe_path);
  //Envia os dados dos pipes ao servidor para que este possa comunicar com o cliente
  write(server_fd, &msg, sizeof(msg));

  return 0;
}
 
int kvs_disconnect(void) {
  Message msg;
  msg.opcode = 2;

  if (write(server_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending disconnect message");
    close(server_fd);
    return -1;
  }
  return 0;
}

int kvs_subscribe(const char* key) {
  Message msg;
  msg.opcode = 3;
  strncpy(msg.key, key, sizeof(msg.key));

  if (write(server_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending subscription message");
    close(server_fd);
    return -1;
  }

  return 0;
}

int kvs_unsubscribe(const char* key) {
  Message msg;
  msg.opcode = 4;
  strncpy(msg.key, key, sizeof(msg.key));

  if (write(server_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending unsubscription message");
    close(server_fd);
    return -1;
  }

  close(server_fd);
  return 0;
}


