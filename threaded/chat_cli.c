#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <wait.h>
#define MSG_SIZE MAX_MSG+NAME_SIZE+4
void inputMsg(char *buffer){
  char c;
  int i=0;
  while((c=getchar())!='\n' && i<MAX_MSG-1){
    buffer[i++]=c;
  }
  buffer[i]='\0';
}

void cleanBuffer(char *buffer)
{
  memset(buffer, 0, MAX_MSG);
  fflush(stdout);
  fflush(stdin); // Necessary for Windows systems
}

int main(){
  struct sockaddr_in srv_addr;
  char buffer[MAX_MSG]={0}, name[NAME_SIZE], input_msg[MSG_SIZE];
  int sock=0, entered_name=0;

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

  printf("Welcome! you can close the chat sending the message %s\n", CLIENT_EXITING_MSG);
  
  pid_t pid = fork();
  if(pid==0){
    while(1){
      cleanBuffer(input_msg);
      recv(sock, input_msg, MSG_SIZE, 0);
      if(strcmp(EXITING_MSG, input_msg)==0){
        printf("The server was shut down. You may exit the chat.\n");
        break;
      }
      printf("%s\n>> ", input_msg);
    }
  }else{
    signal(SIGINT, SIG_IGN);
    while(strcmp(buffer, CLIENT_EXITING_MSG)){
      cleanBuffer(buffer);
      if(entered_name){
        printf(">> ");
        inputMsg(buffer);
      }
      else {
        printf("Input your name: ");
        do{
          inputMsg(buffer);
          strcpy(name, buffer);
        }while(strcmp(buffer, "")==0);
        entered_name=1;
        printf("Welcome %s!\n", name);
      }
      //printf("entered msg: %s\n", buffer);
      if(strlen(buffer)){
        send(sock, buffer, strlen(buffer), 0);
      }
    }
    kill(pid, SIGINT);
    wait(NULL);
    close(sock);
  }
    return 0;
}