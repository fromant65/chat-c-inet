#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
typedef struct Client
{
  pid_t pid_thread;
  int socket_fd;
} Client;

typedef struct Clients
{
  Client list[MAX_CLIENTS];
  int used_indexes[MAX_CLIENTS];
  int used_indexes_count;
} Clients;

Clients clients = {.used_indexes_count = 0};
void *shm_ptr, *broadcast_ptr;


int get_smaller_nonused_index(Clients clients)
{
  int presentes[MAX_CLIENTS] = {0};
  int i;
  for (i = 0; i < clients.used_indexes_count; i++)
  {
    presentes[clients.used_indexes[i]] = 1;
  }
  for (i = 0; presentes[i] && i < MAX_CLIENTS; i++)
    ;
  if (i == MAX_CLIENTS)
    return -1; // No hay indices sin usar
  return i;
}

void insert_client(pid_t pid, int sock_cli)
{
  int smaller_nonused_index = get_smaller_nonused_index(clients);
  Client new_c;
  new_c.pid_thread = pid;
  new_c.socket_fd = sock_cli;
  clients.list[smaller_nonused_index] = new_c;
  clients.used_indexes[clients.used_indexes_count] = smaller_nonused_index;
  clients.used_indexes_count++;
}

void remove_client(pid_t exit_pid)
{
  int i;
  Client *list = clients.list;
  int index = -1;
  // Busco al cliente en la lista por PID y guardo su índice en la misma
  for (i = 0; i < clients.used_indexes_count && index == -1; i++)
  {
    Client current = list[clients.used_indexes[i]];
    if (current.pid_thread == exit_pid)
    {
      printf("Removing client with pid: %d\n", current.pid_thread);
      index = i;
    }
  }
  // Corro todos los indices usados, despues del que hay que borrar, un espacio a la izquierda
  for (int i = index; i < clients.used_indexes_count; i++)
  {
    clients.used_indexes[i] = clients.used_indexes[i + 1];
  }
  clients.used_indexes_count--;
}

void print_client_list(Clients clients)
{
  for (int i = 0; i < clients.used_indexes_count; i++)
  {
    printf("PID: %d\n", clients.list[clients.used_indexes[i]].pid_thread);
  }
}

void cleanBuffer(char *buffer)
{
  memset(buffer, 0, MAX_MSG);
  fflush(stdout);
  fflush(stdin); // Necessary for Windows systems
}


void broadcast(int socket, char* buffer){
  for (int i = 0; i < clients.used_indexes_count; i++)
  {
    Client current = clients.list[clients.used_indexes[i]];
    if(current.socket_fd != socket){
      char message[MAX_MSG+36];
      sprintf(message, "%d: %s", current.pid_thread, buffer);
      message[strlen(message)-1] = '\0';
      printf("Broadcasting \"%s\" to socket n° %d\n", message, current.socket_fd);
      send(current.socket_fd, message, strlen(message), 0);
    }
  }
  printf("Finished broadcast\n");
}

void tokenize_and_broadcast_message(const char *msg){
  // Format: "sock_fd message"
  char fd[10], msg_to_send[MAX_MSG],c;
  cleanBuffer(msg_to_send);
  int i=0, j=0;
  while((c=msg[i])!=' ') 
    fd[i++]=c;
  fd[i] = 0;
  while((c=msg[i++])!='\0') 
    msg_to_send[j++]=c;
  msg_to_send[i]=0;
  int sock_fd = atoi(fd);
  //printf("Tokenized message: %s\nSocket: %d\n",msg_to_send, sock_fd);
  broadcast(sock_fd, msg_to_send);
}

void handleClient(int sock_cli, char *buffer)
{
  while (strcmp(buffer, "EXIT"))
  {
    int msg_size = recv(sock_cli, buffer, MAX_MSG, 0);
    if (msg_size > 0)
    {
      printf("Received from client (size %d): %s\n",msg_size, buffer);
      if (strcmp(buffer, "EXIT") == 0)
        continue;
      sprintf((char *)broadcast_ptr, "%d %s", sock_cli, buffer);
      kill(getppid(), SIGUSR1);
      cleanBuffer(buffer);
    }
  }
}

void setup_shm(int*shm_fd){
  *shm_fd = shm_open(MEMORY_NAME, O_CREAT | O_RDWR, 0666);
  if (*shm_fd == -1)
  {
    perror("Error al crear memoria compartida");
    exit(EXIT_FAILURE);
  }

  if (ftruncate(*shm_fd, SH_MEMORY) == -1)
  {
    perror("Error al configurar tamaño de memoria compartida");
    exit(EXIT_FAILURE);
  }

  shm_ptr = mmap(0, SH_MEMORY, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
  if (shm_ptr == MAP_FAILED)
  {
    perror("Error al mapear memoria compartida");
    exit(EXIT_FAILURE);
  }
}

void setup_broadcast(int* broadcast_fd){
  *broadcast_fd = shm_open(BROADCAST_NAME, O_CREAT | O_RDWR, 0666);
  if (*broadcast_fd == -1)
  {
    perror("Error al crear memoria compartida");
    exit(EXIT_FAILURE);
  }

  if (ftruncate(*broadcast_fd, BROADCAST_MEMORY) == -1)
  {
    perror("Error al configurar tamaño de memoria compartida");
    exit(EXIT_FAILURE);
  }

  broadcast_ptr = mmap(0, BROADCAST_MEMORY, PROT_READ | PROT_WRITE, MAP_SHARED, *broadcast_fd, 0);
  if (broadcast_ptr == MAP_FAILED)
  {
    perror("Error al mapear memoria compartida");
    exit(EXIT_FAILURE);
  }
}

void handler(int signum)
{
  if(signum==SIGCHLD){
    char buffer[SH_MEMORY];
    sprintf(buffer, "%s", (char *)shm_ptr);
    pid_t pid = atoi(buffer);
    printf("Child terminated with pid %d\n", pid);
    remove_client(pid);
  }if(signum==SIGUSR1){
    char buffer[BROADCAST_MEMORY];
    sprintf(buffer, "%s", (char *)broadcast_ptr);
    printf("%s\n", (char*)broadcast_ptr);
    printf("Broadcasting message of length %lu: %s\n",strlen(buffer), buffer);
    tokenize_and_broadcast_message(buffer);
  }
}

int main()
{
  int sock_srv, sock_cli;
  struct sockaddr_in srv_name;
  int adrrlen = sizeof(srv_name);
  int opt = 1;

  // Shared Memory for sockets
  int shm_fd;
  setup_shm(&shm_fd);
  
  // Broadcast Memory
  int broadcast_fd;
  setup_broadcast(&broadcast_fd);

  // Signals
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1 || sigaction(SIGUSR1, &sa, NULL) == -1)
  {
    perror("sigaction");
    exit(1);
  }

  // Create socket fd
  if ((sock_srv = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }
  printf("Sock srv: %d\n", sock_srv);
  // Attaching socket to port
  if (setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
  {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  srv_name.sin_family = AF_INET;
  srv_name.sin_port = htons(PORT);
  srv_name.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock_srv, (struct sockaddr *)&srv_name, sizeof(srv_name)) < 0)
  {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(sock_srv, MAX_CLIENTS) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Listening on PORT %d\n", PORT);
  while (1)
  {
    //printf("Connection incoming...\n");
    if ((sock_cli = accept(sock_srv, (struct sockaddr *)&srv_name, (socklen_t *)&adrrlen)) < 0)
    {
      perror("accept");
      exit(EXIT_FAILURE);
    }
    printf("New connection accepted: %d\n", srv_name.sin_addr.s_addr);
    int pid = fork();
    if (pid == 0)
    {
      signal(SIGUSR1, SIG_IGN);
      char buffer[MAX_MSG] = {0};
      handleClient(sock_cli, buffer);
      close(sock_cli);
      
      sprintf((char *)shm_ptr, "%d", getpid());
      exit(0);
    }
    else
    {
      insert_client(pid, sock_cli);
      printf("New client. Client list:\n");
      print_client_list(clients);
    }
  }
  close(sock_srv);
  munmap(shm_ptr, SH_MEMORY);
  shm_unlink(MEMORY_NAME);

  return 0;
}