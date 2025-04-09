#include "clients.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
Client_list new_list(){
  Client_list list = {.head=NULL, .tail=NULL};
  return list;
}
Client_list add_client(char *name, int socket_fd, Client_list clients, pthread_t *thread){
  Client_node *new_client = malloc(sizeof(Client_node));
  if (new_client == NULL) {
      perror("Failed to allocate memory for new client");
      exit(EXIT_FAILURE);
  }
  strcpy(new_client->current.name, name);
  new_client->current.socket_fd = socket_fd;
  new_client->current.thread = thread;
  new_client->next = NULL;

  if (clients.head == NULL) {
      clients.head = new_client;
      clients.tail = new_client;
  } else {
      clients.tail->next = new_client;
      clients.tail = new_client;
  }
  return clients;
}

Client_list remove_client(int socket_fd, Client_list clients){
  Client_node *current = clients.head;
  Client_node *previous = NULL;

  while (current != NULL) {
      if (current->current.socket_fd == socket_fd) {
          if (previous == NULL) {
              clients.head = current->next;
          } else {
              previous->next = current->next;
          }
          if (current == clients.tail) {
              clients.tail = previous;
          }
          free(current);
          break;
      }
      previous = current;
      current = current->next;
  }
  return clients;
}
void print_client_list(Client_list clients){
  Client_node *current = clients.head;
  printf("Client list:\n");
  while (current != NULL) {
      printf("Name: %s, Socket FD: %d\n", current->current.name, current->current.socket_fd);
      current = current->next;
  }
  printf("\n");
}