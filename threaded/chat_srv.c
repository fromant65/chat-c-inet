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

pthread_mutex_t msg_mutex=PTHREAD_MUTEX_INITIALIZER;
char msg[MAX_MSG];

void* handleClient(void** args);
void cleanBuffer(char *buffer);

int main(){
  struct sockaddr_in srv_name;
  int addrlen = sizeof(srv_name);
  int sock_srv, sock_client;
  int opt = 1;
  Client_list clients=new_list();
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

  //Accepting clients
  while(1){
    if((sock_client=accept(sock_srv, (struct sockaddr *)&srv_name, (socklen_t *)&addrlen))<0){
      perror("accept");
      exit(EXIT_FAILURE);
    }
    printf("New connection accepted: %d\n", srv_name.sin_addr.s_addr);
    pthread_t *thread = malloc(sizeof(pthread_t));
    void** args = malloc(sizeof(void*)*2);
    args[0]=&sock_client;
    args[1]=&clients;
    if(pthread_create(thread, NULL, (void *)handleClient, args) != 0){
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
  while (strcmp(buffer, "EXIT"))
  {
    int msg_size = recv(sock_cli, buffer, MAX_MSG, 0);
    if(msg_size>0 && recieved_name==0){
      printf("Updating client name\n");
      char name[NAME_SIZE];
      sscanf(buffer, "%s", name);
      printf("New name: %s\n", name);
      for(Client_node *c = clients.head; c!=NULL; c=c->next){
        if(c->current.socket_fd == sock_cli){
          strcpy(c->current.name, name);
          break;
        }
      }
      recieved_name=1;
    }
    else if (msg_size > 0)
    {
      char name[NAME_SIZE];
      for(Client_node *c = clients.head; c!=NULL; c=c->next){
        if(c->current.socket_fd == sock_cli){
          strcpy(name, c->current.name);
          break;
        }
      }
      printf("Received from client %s (size %d): %s\n",name,msg_size, buffer);
      if (strcmp(buffer, "EXIT") == 0)
        continue;
      pthread_mutex_lock(&msg_mutex);
      strcpy(msg, buffer);
      msg[strlen(msg)] = '\0';
      printf("Broadcasting \"%s\" to all clients\n", msg);
      // Broadcast to all clients
      for (Client_node *c = clients.head; c!=NULL; c=c->next)
      {
        Client_data current = c->current;
        if(current.socket_fd != sock_cli){
          char message[MAX_MSG+NAME_SIZE+4];
          sprintf(message, "%s: %s", name, msg);
          message[strlen(message)] = '\0';
          printf("Broadcasting \"%s\" to socket n° %d\n", message, current.socket_fd);
          send(current.socket_fd, message, strlen(message), 0);
        }
      }
      pthread_mutex_unlock(&msg_mutex);
      cleanBuffer(buffer);
    }
  }
  return NULL;
}