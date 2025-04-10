#include <pthread.h>
#include "../settings.h"
typedef struct Client
{
  pthread_t *thread;
  char name[NAME_SIZE];
  int socket_fd;
} Client_data;

typedef struct Clients_
{
  Client_data current;
  struct Clients_ *next;
} Client_node;

typedef struct
{
  Client_node *head;
  Client_node *tail;
} Client_list;

Client_list new_list();
Client_list add_client(char *name, int socket_fd, Client_list clients, pthread_t *thread);
Client_list remove_client(int socket_fd, Client_list clients);
void print_client_list(Client_list clients);
void get_client_name_by_socket(Client_list clients, int socket, char* name_buffer);
void update_client_name_by_socket(Client_list clients, int socket, char* name_buffer);