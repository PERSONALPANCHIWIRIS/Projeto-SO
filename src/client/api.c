#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <fcntl.h>

int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe) {
  // Criar os FIFO's do cliente
  if (mkfifo(req_pipe_path, 0666) == -1) {
      perror("Erro ao criar FIFO de pedidos");
      return 1;
  }
  if (mkfifo(resp_pipe_path, 0666) == -1) {
      perror("Erro ao criar FIFO de respostas");
      return 1;
  }
  if (mkfifo(notif_pipe_path, 0666) == -1) {
      perror("Erro ao criar FIFO de notificações");
      return 1;
  }

  // Enviar mensagem de conexão para o servidor
  int server_fd = open(server_pipe_path, O_WRONLY);
  if (server_fd == -1) {
      perror("Erro ao abrir FIFO do servidor");
      return 1;
  }

  char buffer[256]; 
  snprintf(buffer, sizeof(buffer), "%s|%s|%s", req_pipe_path, resp_pipe_path, notif_pipe_path);
  //Envia os dados dos pipes ao servidor para que este possa comunicar com o cliente
  write(server_fd, buffer, strlen(buffer) + 1);
  close(server_fd);

  // Abrir o FIFO de notificações
  *notif_pipe = open(notif_pipe_path, O_RDONLY | O_NONBLOCK);
  if (*notif_pipe == -1) {
    perror("Erro ao abrir FIFO de notificações");
    return 1;
  }

  return 0;
}
 
int kvs_disconnect(void) {
  // close pipes and unlink pipe files
  return 0;
}

int kvs_subscribe(const char* key) {
  // send subscribe message to request pipe and wait for response in response pipe
  return 0;
}

int kvs_unsubscribe(const char* key) {
    // send unsubscribe message to request pipe and wait for response in response pipe
  return 0;
}


