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
  notif_pipe++; //Depois vemos o que fazemos com isto

  // Enviar mensagem de conexão para o servidor
  Message msg;
  msg.opcode = 1;
  snprintf(msg.data, sizeof(msg.data), "%s|%s|%s", req_pipe_path, resp_pipe_path, notif_pipe_path);
  //Envia os dados dos pipes ao servidor para que este possa comunicar com o cliente
  write(server_fd, &msg, sizeof(msg));

  //Le a mesnagem de connect com sucesso
  int client_resp_fd = open(resp_pipe_path, O_RDONLY);
  char response[MAX_STRING_SIZE];
  ssize_t bytes_read = read(client_resp_fd, response, sizeof(response));

  if (bytes_read <= 0) {
    perror("Erro ao ler do FIFO de response");
    close(client_resp_fd);
    return -1;
  }

  close(client_resp_fd);
  response[bytes_read] = '\0'; //Garantir que acaba em \0
  fprintf(stdout, "%s\n", response);
  return 0;
}
 
int kvs_disconnect(const char* req_pipe_path, const char* resp_pipe_path) {
  Message msg;
  msg.opcode = 2;
  
  //Comunca com o server e envia a mensagem de disconnect
  int client_req_fd = open(req_pipe_path, O_WRONLY);
  if (write(client_req_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending disconnect message");
    close(client_req_fd);
    return 1;
  }
  close(client_req_fd);

  //Le a mesnagem de disconnect com sucesso
  int client_resp_fd = open(resp_pipe_path, O_RDONLY);
  char response[MAX_STRING_SIZE];
  ssize_t bytes_read = read(client_resp_fd, response, sizeof(response));
  if (bytes_read <= 0) {
    perror("Erro ao ler do FIFO de response");
    close(client_resp_fd);
    return -1;
  }
  close(client_resp_fd);
  response[bytes_read] = '\0'; //Garantir que acaba em \0
  fprintf(stdout, "%s", response);

  //Depois de estar todo fechado do lado do server, fecha no cliente
  close(server_fd);
  return 0;
}

int kvs_subscribe(const char* key, const char* req_pipe_path, const char* resp_pipe_path) {
  Message msg;
  msg.opcode = 3;
  strncpy(msg.key, key, sizeof(msg.key));

  int client_req_fd = open(req_pipe_path, O_WRONLY);
  if (write(client_req_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending subscription message");
    close(client_req_fd);
    return -1;
  }
  close(client_req_fd);

  //Le a mesnagem de subscrição com sucesso
  int client_resp_fd = open(resp_pipe_path, O_RDONLY);
  char response[MAX_STRING_SIZE];
  ssize_t bytes_read = read(client_resp_fd, response, sizeof(response));
  if (bytes_read <= 0) {
    perror("Erro ao ler do FIFO de response");
    close(client_resp_fd);
    return -1;
  }
  close(client_resp_fd);
  response[bytes_read] = '\0'; //Garantir que acaba em \0
  fprintf(stdout, "%s", response);

  return 0;
}

int kvs_unsubscribe(const char* key, const char* req_pipe_path, const char* resp_pipe_path) {
  Message msg;
  msg.opcode = 4;
  strncpy(msg.key, key, sizeof(msg.key));

  int client_req_fd = open(req_pipe_path, O_WRONLY);
  if (write(client_req_fd, &msg, sizeof(msg)) < 0) {
    perror("Error sending unsubscription message");
    close(client_req_fd);
    return -1;
  }
  close(client_req_fd);

  //Le a mesnagem de unsubscribe com sucesso
  int client_resp_fd = open(resp_pipe_path, O_RDONLY);
  char response[MAX_STRING_SIZE];
  ssize_t bytes_read = read(client_resp_fd, response, sizeof(response));
  if (bytes_read <= 0) {
    perror("Erro ao ler do FIFO de response");
    close(client_resp_fd);
    return -1;
  }
  close(client_resp_fd);
  response[bytes_read] = '\0'; //Garantir que acaba em \0
  fprintf(stdout, "%s", response);
  
  return 0;
}


