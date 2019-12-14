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
#include <sodium.h>


//get files from ./server_files
#include <dirent.h>
#include <sys/stat.h>

#define SERVER_PORT     9000
#define BUF_SIZE 1024

typedef unsigned char byte;



void erro(char *msg);

int main(int argc, char *argv[]){
  //SODIUM LIB
  unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
  unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];
  
  char endServer[100];
  char buffer[BUF_SIZE];
  char buffer_to_rcv[BUF_SIZE];
  char aux[BUF_SIZE];
  int fdTCP,fdUDP;
  struct sockaddr_in server_addr;
  struct hostent *hostPtr;
  int command_lenght = 0;
  int nread;
  char* token;
  char** split_command;
  socklen_t size_adr;

  //Init sodium
  if(sodium_init()<0){
    erro("Couldn't init sodium");
    exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    	printf("Nao consegui obter endereço\n");

  //LIMPA addr
  bzero((void *) &server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  server_addr.sin_port = htons(atoi(argv[2]));
  size_adr = sizeof(server_addr);
  //UDP SOCKET
  if ((fdUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("socket creation failed");
    exit(0);
  }
  //TCP SOCKET
  if((fdTCP = socket(AF_INET,SOCK_STREAM,0)) == -1){
    erro("socket");
  }

  if( connect(fdTCP,(struct sockaddr *)&server_addr,sizeof (server_addr)) < 0){
    erro("Connect");
  }

  while(1){
    command_lenght = 0;
    printf("COMMAND: ");
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strlen(buffer)-1] = '\0'; //removing \n
    //SPLIT COMMAND
    strcpy(aux,buffer);
    split_command = (char**)calloc(1,sizeof(char*));
    token = strtok(aux," ");

    while(token){
      split_command = (char**)realloc(split_command,(command_lenght+1)*sizeof(char*));
      split_command[command_lenght] = (char*)calloc(strlen(token)+1,sizeof(char));
      strcpy(split_command[command_lenght],token);
      token = strtok(NULL, " ");
      command_lenght++;
    }

    if(command_lenght>0){
      //LEU ALGO
      if(strcmp(buffer,"LIST") == 0){
        write(fdTCP, buffer, sizeof(buffer));
        while(strcmp(buffer,"#") != 0){
          nread = read(fdTCP,buffer,sizeof(buffer));
          buffer[nread] = '\0';
          if(strcmp(buffer,"#"))
            printf("\t%s\n",buffer );
        }
        //clear buffer from proxy
        memset(buffer,0,strlen(buffer));
      }
      else if(strcmp(buffer,"QUIT") == 0){
        write(fdTCP, buffer, sizeof(buffer));
        break;
      }
      else if(command_lenght == 4){
        if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"UDP") == 0){
          //write(fdTCP, "UDP", 5*sizeof(char));

          //verify if file exists
          write(fdTCP, split_command[2], sizeof(split_command[2]));

          // wait to check if file was found
          nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
          buffer_to_rcv[nread] = '\0';
          printf("%s\n", buffer_to_rcv);
          if(strcmp(buffer_to_rcv,"OK") == 0){
            struct timeval tv = {1,0};
            sendto(fdUDP, (const char*)split_command[2], strlen(split_command[2]), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
            FILE* f = fopen(split_command[2],"wb");

            fd_set fdset;
            FD_ZERO(&fdset);

            //sai quando não existir nada para receber
            while(1){
              FD_SET(fdUDP,&fdset);
              int not_empty = select(fdUDP+1,&fdset,NULL,NULL,&tv);
              printf("EMPTY %d\n", not_empty);
              if(not_empty){
                nread = recvfrom(fdUDP, buffer_to_rcv, sizeof(byte)*BUF_SIZE, 0,NULL, NULL);
                printf("%d\n", nread);
                fwrite(buffer_to_rcv,sizeof(byte),nread,f);
              }
              else{
                break;
              }
            }

            fclose(f);
            printf("%s asdasdad\n", buffer);
          }
          else{
            printf("Received a File not found error.\n");
          }


        }
        else if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"TCP") == 0){
          write(fdTCP, split_command[2], sizeof(split_command[2]));

          // wait to check if file was found
          nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
          buffer_to_rcv[nread] = '\0';
          printf("%s\n", buffer_to_rcv);
          if(strcmp(buffer_to_rcv,"OK") == 0){
            FILE* f = fopen(split_command[2],"wb");
            while(1){
              printf("bbb\n");
              nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
              printf("%d\n", nread);
              fwrite(buffer_to_rcv,sizeof(byte),nread,f);
              if(nread != BUF_SIZE){
                break;
              }
            }
            fclose(f);
          }
          else{
            printf("Received a File not found error.\n");
          }

        }
      }
    }
    free(split_command);
  }

}


void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}
