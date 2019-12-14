/**********************************************************************
 * CLIENTE liga ao servidor (definido em argv[1]) no porto especificado
 * (em argv[2]), escrevendo a palavra predefinida (em argv[3]).
 * USO: >cliente <enderecoServidor>  <porto>  <Palavra>
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

int fdUDP;

#define BUFFER_SIZE 1024

void erro(char *msg);

int main(int argc, char *argv[]) {
  char endServer[100];
  int fd;
  char buffer[BUFFER_SIZE];
  char aux[BUFFER_SIZE];
  char buffer_to_rcv[BUFFER_SIZE];
  char* token;
  char** split_command;
  int n;
  int i;
  struct sockaddr_in addr;
  struct hostent *hostPtr;

  if (argc != 5) {
    	printf("cliente <proxy> <host> <port> <protocolo>\n");
    	exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    	erro("Nao consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) atoi(argv[3]));

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	 erro("socket");
  if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
	 erro("Connect");

   //UDP
  if((fdUDP = socket(AF_INET,SOCK_DGRAM,0)) == -1)
    erro("socket");

  //Passa o ip do servidor
  write(fd, argv[2], 1 + strlen(argv[2]));
  while(1){
    i = 0;
    printf("COMMAND: ");
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strlen(buffer)-1] = '\0'; //removing \n
    //SPLIT COMMAND
    strcpy(aux,buffer);
    split_command = (char**)calloc(1,sizeof(char*));
    token = strtok(aux," ");

    while(token){
      split_command = (char**)realloc(split_command,(i+1)*sizeof(char*));
      split_command[i] = (char*)calloc(strlen(token)+1,sizeof(char));
      strcpy(split_command[i],token);
      token = strtok(NULL, " ");
      i++;
    }

    if(i>0){
      //LEU ALGO
      if(strcmp(buffer,"LIST") == 0){
        write(fd, buffer, sizeof(buffer));
        while(strcmp(buffer_to_rcv,"#") != 0){
          n = read(fd,buffer_to_rcv,sizeof(buffer_to_rcv));
          buffer_to_rcv[n] = '\0';
          if(strcmp(buffer_to_rcv,"#"))
            printf("\t%s\n",buffer_to_rcv );
        }
        //clear buffer from proxy
        memset(buffer_to_rcv,0,strlen(buffer_to_rcv));
      }
      else if(strcmp(buffer,"QUIT") == 0){
        write(fd, buffer, sizeof(buffer));
        break;
      }
      else if(i == 4){
        if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"UDP") == 0){
          write(fd, "UDP", 5*sizeof(char));
          write(fdUDP,"asdasd",5*sizeof(char));

        }
        else if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"TCP") == 0){
          write(fd, "TCP", 5*sizeof(char));
        }
        else if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"UDP") == 0){
          printf("aaa\n");
          sendto(fdUDP, (const char *)"hello", 5,0, (const struct sockaddr *) &addr,sizeof(addr));
        }
      }
    }
    free(split_command);
  }
  close(fd);
  exit(0);
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}
