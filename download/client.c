#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

//get files from ./server_files
#include <dirent.h>
#include <sys/stat.h>

#define SERVER_PORT     9000
#define BUF_SIZE 1024

int main(int argc, char *argv[]){
  char endServer[100];
  int sockfd,fdUDP, client;
  fd_set fdset;
  struct sockaddr_in server_addr;
  struct hostent *hostPtr;
  char message[] = "helol\n";

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    	printf("Nao consegui obter endereço\n");

  //LIMPA addr
  bzero((void *) &server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  server_addr.sin_port = htons(atoi(argv[2]));

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket creation failed");
        exit(0);
    }

  sendto(sockfd, (const char*)message, strlen(message), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
}
