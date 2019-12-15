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

#define BUFFER_SIZE 1024
#define SERVER_PORT 9002

int fdUDP;

void erro(char *msg);
void process_client(int fd_client, char* port);
void handle_udp_requests();

void process(int fd_client, int fd_server){
  //Socket para enviar pacotes ao servidor
  char endServer[100];
  struct sockaddr_in addr_server;
  struct hostent *hostPtr;

  //Le comando passado pelo cliente
  //O primeiro connect -> buffer == ip host
  char buffer[BUFFER_SIZE];
  char buffer_server[BUFFER_SIZE];
  int n;
  while(1){
    n = read(fd_server, buffer,sizeof(buffer));
    buffer[n] = '\0';
    write(fd_client,buffer,sizeof(buffer));
    if(strcmp(buffer,"QUIT") == 0){
      break;
    }
    // else if(strcmp(buffer,"LIST") == 0){
    //   while(strcmp(buffer_server,"#")){
    //     n = read(fd_server,buffer_server,sizeof(buffer_server));
    //     buffer_server[n] = '\0';
    //     printf("%d %s\n",n, buffer_server);
    //     write(fd_client,buffer_server,sizeof(buffer_server));
    //   }
    //   //clear buffer from server
    //   memset(buffer_server,0,strlen(buffer_server));
    // }
  }
  close(fd_server);

  exit(0);
}

int main(int argc, char *argv[]) {
  int fd, client;
  struct sockaddr_in addr, client_addr;
  int client_addr_size;

  if (argc != 2) {
    	printf("proxy <port>\n");
    	exit(-1);
  }

  //LIMPA addr
  bzero((void *) &addr, sizeof(addr));
  //Socket para receber requests do cliente
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.2");
  addr.sin_port = htons(atoi(argv[1]));

  if ( (fdUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("ERRO");
  if ( bind(fdUDP,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	  printf("ERRO");

  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	 printf("ERRO");
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	 printf("ERRO");
  if( listen(fd, 5) < 0)
  	printf("na funcao listen");

  client_addr_size = sizeof(client_addr);
  while (1) {
    //clean finished child processes, avoiding zombies
    //must use WNOHANG or would block whenever a child process was working
      while(waitpid(-1,NULL,WNOHANG)>0);
      //wait for new connection
      client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
      if (client > 0) {
        if (fork() == 0) {
          close(fd);
          process_client(client, argv[1]);
          exit(0);
        }
      close(client);
      }
  }
  return 0;


  exit(0);
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}

void process_client(int fd_client, char* port){
  //Socket para enviar pacotes ao servidor
  char endServer[100];
  int fd_server;
  struct sockaddr_in addr_server;
  struct hostent *hostPtr;

  //Le comando passado pelo cliente
  //O primeiro connect -> buffer == ip host
  char buffer[BUFFER_SIZE];
  char buffer_server[BUFFER_SIZE];
  int n;
  n = read(fd_client, buffer,sizeof(buffer));
  buffer[n] = '\0';
  printf("PROXY : %s\n", buffer);

  strcpy(endServer, buffer);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    	erro("Nao consegui obter endereço");

  //Limpa addr
  bzero((void *) &addr_server, sizeof(addr_server));
  addr_server.sin_family = AF_INET;
  addr_server.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr_server.sin_port = htons((short) atoi(port));
  //Socket para enviar pacotes para o servidor esta criado



  //cria socket para proxy<->server
  if((fd_server = socket(AF_INET,SOCK_STREAM,0)) == -1)
	 erro("socket");
  if( connect(fd_server,(struct sockaddr *)&addr_server,sizeof (addr_server)) < 0)
	 erro("Connect");

  //Conectado com o servidor
  if (fork() == 0) {
    process(fd_client, fd_server);
  }
  while(1){
    n = read(fd_client, buffer,sizeof(buffer));
    buffer[n] = '\0';
    write(fd_server,buffer,sizeof(buffer));
    if(strcmp(buffer,"QUIT") == 0){
      break;
    }
    // else if(strcmp(buffer,"LIST") == 0){
    //   while(strcmp(buffer_server,"#")){
    //     n = read(fd_server,buffer_server,sizeof(buffer_server));
    //     buffer_server[n] = '\0';
    //     printf("%d %s\n",n, buffer_server);
    //     write(fd_client,buffer_server,sizeof(buffer_server));
    //   }
    //   //clear buffer from server
    //   memset(buffer_server,0,strlen(buffer_server));
    // }
  }
  close(fd_server);

  exit(0);
}


void handle_udp_requests(){
  struct sockaddr_in client_addr;
  char buffer[BUFFER_SIZE];
  socklen_t client_addr_size = sizeof(client_addr);
  int nread;
  printf("aaaaa\n");
  nread = recvfrom(fdUDP,buffer,sizeof(buffer),0,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
  buffer[nread] = '\0';
  printf("%s\n", buffer);
  printf("aaaaa\n");
  exit(0);
}
