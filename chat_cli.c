#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void inputMsg(char *buffer){
  char c;
  int i=0;
  while((c=getchar())!='\n' && i<MAX_MSG-1){
    buffer[i++]=c;
  }
  buffer[i]='\0';
}

int main(){
  struct sockaddr_in srv_addr;
  char buffer[MAX_MSG]={0}, name[32], output_msg[MAX_MSG+36], input_msg[MAX_MSG+36];
  int sock=0;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(PORT);

  if(inet_pton(AF_INET, ADDRESS,&srv_addr.sin_addr )<=0){
    printf("Invalid IPv4 address\n");
    return -1;
  }

  if(connect(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr))<0){
    printf("Connection Failed\n");
    return -1;
  }

  printf("Ingresa tu nombre: ");
  inputMsg(name);

  pid_t pid = fork();
  if(pid==0){
    while(1){
      recv(sock, input_msg, MAX_MSG, 0);
      printf("\n> %s\n>>", input_msg);
    }
  }else{
    while(strcmp(buffer, "EXIT")){
      printf(">> ");
      inputMsg(buffer);
      if(strlen(buffer)){
        if(strcmp(buffer, "EXIT"))
        sprintf(output_msg, "%s", buffer);
        else
        sprintf(output_msg, "%s", buffer);
        send(sock, output_msg, strlen(buffer), 0);
        //printf(">> %s\n", output_msg);
        //recv(sock, buffer, MAX_MSG, 0);
        //printf("> %s\n", buffer);
      }
    }
    close(sock);
  }
    return 0;
  }