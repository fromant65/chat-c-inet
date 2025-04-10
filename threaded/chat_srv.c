/**
 * Arquitectura del servidor:
 * Se levanta un hilo para cada cliente
 * Para recibir los mensajes usamos 
 * char msg[MAX_MSG];
 * como variable global. Esta se vuelve un area critica del servidor
 * ----
 * Cada hilo queda a la espera de un mensaje del cliente que tiene asignado. 
 * Cuando llega un mensaje, lo escribe en msg
 * Luego, debe hacer un broadcast al resto de hilos
 * Clientes clientes[MAX_CLIENTES] <-- Guardamos la lista de los fd de cada cliente 
 * para hacer un send con el mensaje a cada uno
 * Luego, desbloqueamos el area critica
 * ----
 * Ingreso de clientes:
 * Cuando un cliente se conecta, se le asigna un fd y se guarda en la lista de clientes.
 * Cuando un cliente se desconecta, se elimina de la lista de clientes
 * En lugar de representar la lista de clientes con un array, 
 * la podemos representar con una lista simplemente enlazada
 *    Al ingresar un cliente, se apila
 *    Al desconectarse un cliente, se elimina el nodo y se enlazan sus vecinos
 *    La lista de clientes se usa para broadcasts, por lo que siempre se requiere hacer un recorrido lineal
 * Un cliente tiene:
 *    fd: un file descriptor del socket
 *    nombre: un nombre de usuario para identificarlo
 *    
 */

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
#include <pthread.h>
#include "srv_utils/clients.h"
#include <sys/select.h>

pthread_mutex_t clients_mutex=PTHREAD_MUTEX_INITIALIZER;

void* handleClient(void** args);
void cleanBuffer(char *buffer);
void* commands(Client_list* clients);
void inputCmd(char *buffer);
int main(){
  struct sockaddr_in srv_name;
  int addrlen = sizeof(srv_name);
  int sock_srv, sock_client;
  int opt = 1;
  Client_list clients=new_list();
  signal(SIGINT, SIG_IGN);
  if((sock_srv=socket(AF_INET, SOCK_STREAM, 0))==0){
    perror("socket");
    exit(EXIT_FAILURE);
  }

  if(setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  srv_name.sin_family = AF_INET;
  srv_name.sin_addr.s_addr = htonl(INADDR_ANY);
  srv_name.sin_port = htons(PORT);

  if(bind(sock_srv,(struct socketaddr *)&srv_name,sizeof(srv_name))<0){
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if(listen(sock_srv, MAX_CLIENTS)<0){
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Listening on PORT %d\n", PORT);
  //Incluir este thread hace que el server se bloquee rapidamente esperando comandos
  // Habria que manejar los scanf para los comandos de forma no bloqueante
  // printf("Write !help to get available server commands\n");
  // pthread_t command_thread;
  // pthread_create(&command_thread, NULL, (void*)commands, (void*)&clients);
  //Accepting clients
  while(1){
    if((sock_client=accept(sock_srv, (struct sockaddr *)&srv_name, (socklen_t *)&addrlen))<0){
      perror("accept");
      exit(EXIT_FAILURE);
    }
    printf("[SRV] New connection accepted: %d\n", srv_name.sin_addr.s_addr);
    pthread_t *thread = malloc(sizeof(pthread_t));
    void** args = malloc(sizeof(void*)*2);
    args[0]=&sock_client;
    args[1]=&clients;
    if(pthread_create(thread, NULL, (void*)handleClient, args) != 0){
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
    clients = add_client("", sock_client, clients, thread);
    print_client_list(clients);
  }
}

void cleanBuffer(char *buffer)
{
  memset(buffer, 0, MAX_MSG);
  fflush(stdout);
  fflush(stdin); // Necessary for Windows systems
}

void* handleClient(void** args){
  int sock_cli = *(int*)args[0];
  Client_list clients = *(Client_list*)args[1];
  char buffer[MAX_MSG] = {0};
  int recieved_name = 0;
  while (strcmp(buffer, CLIENT_EXITING_MSG))
  {
    int msg_size = recv(sock_cli, buffer, MAX_MSG, 0);
    if(msg_size>0 && recieved_name==0){
      printf("[CLI] Updating client name\n");
      char name[NAME_SIZE];
      sscanf(buffer, "%s", name);
      printf("[CLI] New name: %s\n", name);
      pthread_mutex_lock(&clients_mutex);
      update_client_name_by_socket(clients, sock_cli, name);
      print_client_list(clients);
      pthread_mutex_unlock(&clients_mutex);
      recieved_name=1;
    }
    else if (msg_size > 0)
    {
      if (strcmp(buffer, CLIENT_EXITING_MSG) == 0)
        continue;

      char name[NAME_SIZE];
      pthread_mutex_lock(&clients_mutex);
      get_client_name_by_socket(clients,sock_cli, name);
      pthread_mutex_unlock(&clients_mutex);
      printf("[CLI] Received from client %s (size %d): %s\n",name,msg_size, buffer);

      // Broadcast to all clients
      pthread_mutex_lock(&clients_mutex);
      for (Client_node *c = clients.head; c!=NULL; c=c->next)
      {
        Client_data current = c->current;
        if(current.socket_fd != sock_cli){
          char message[MAX_MSG+NAME_SIZE+4];
          sprintf(message, "%s: %s", name, buffer);
          message[strlen(message)] = '\0';
          printf("[CLI] Broadcasting \"%s\" to socket n° %d\n", message, current.socket_fd);
          send(current.socket_fd, message, strlen(message), 0);
        }
      }
      pthread_mutex_unlock(&clients_mutex);
      cleanBuffer(buffer);
    }
  }
  //Client terminated, delete from client list
  printf("[CLI] Client with socket %d exited the server\n", sock_cli);
  pthread_mutex_lock(&clients_mutex);
  clients = remove_client(sock_cli, clients);
  *(Client_list*)args[1] = clients;
  print_client_list(clients);
  pthread_mutex_unlock(&clients_mutex);
  close(sock_cli);
  return NULL;
}

void* commands(Client_list* clients){
  char command[32]={0};
  while(strcmp(command, "!exit")){
    printf(">> ");
    inputCmd(command);
    printf("Inserted command: %s\n", command);
    if(strcmp(command, "!help")==0){
      printf("List of available commands:\n");
      printf("!exit: quits the server.\n");
      printf("!clients: shows list of clients.\n");
    }else if(strcmp(command, "!clients")==0){
      print_client_list(*clients);
    }else if(strcmp(command, "!exit")==0){
      pthread_mutex_lock(&clients_mutex);
      for(Client_node *c = clients->head; c!=NULL; c=c->next){
        send(c->current.socket_fd, EXITING_MSG, strlen(EXITING_MSG),0);
      }
      pthread_mutex_unlock(&clients_mutex);
      signal(SIGINT, SIG_DFL);
      raise(SIGINT);
    }else{
      printf("Inserted command \"%s\" not available.\nTry !help to view a list of available commands\n", command);
    }
  }
  return NULL;
}

void inputCmd(char *buffer){
  char c;
  int i=0;
  while((c=getchar())!='\n' && i<MAX_MSG-1){
    buffer[i++]=c;
  }
  buffer[i]='\0';
}
