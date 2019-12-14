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
#define FILE_PATH_SIZE 512
#define BUF_SIZE 1024
#define DIR_NAME "server_files"
#define DEBUG

void manage_udp_download();
void manage_tcp(int client_fd);
int fdTCP,fdUDP;

int main(int argc, char *argv[]){
  int client;
  fd_set fdset;
  struct sockaddr_in addr;
  struct sockaddr_in client_addr;
  int client_addr_size;
  char buffer[BUF_SIZE];
  int nread;

  //LIMPA addr
  bzero((void *) &addr, sizeof(addr));
  //
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(atoi(argv[1]));

  if ( (fdTCP = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	 printf("ERRO");
  if ( bind(fdTCP,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	 printf("ERRO");
  if( listen(fdTCP, 5) < 0)
  	printf("na funcao listen");

  if ( (fdUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("ERRO");
  if ( bind(fdUDP,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	  printf("ERRO");

  //MULTIPLEX para 2 fds
  FD_ZERO(&fdset);
  while(1){
    FD_SET(fdTCP,&fdset);
    FD_SET(fdUDP,&fdset);
    //espera por um connect, seja udp ou TCP
    select(fdUDP+1,&fdset,NULL,NULL,NULL);
    if(FD_ISSET(fdTCP,&fdset)){
      client_addr_size = sizeof(client_addr);
      client = accept(fdTCP,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
      if (client > 0) {
        if (fork() == 0) {
          close(fdTCP);
          manage_tcp(client);
          exit(0);
        }
      close(client);
      }
    }
    if(FD_ISSET(fdUDP,&fdset)){
      manage_udp_download();
    }
  }
}

void manage_udp_download(){
  char buffer[BUF_SIZE];
  socklen_t size_adr;
  struct sockaddr_in client_addr;
  int  nread;
  #ifdef DEBUG
  printf("UDP\n");
  #endif
  size_adr = sizeof(client_addr);
  nread = recvfrom(fdUDP, buffer, sizeof(buffer), 0,(struct sockaddr*)&client_addr, &size_adr);
  buffer[nread] = '\0';
}

void manage_tcp(int client_fd){
  int nread = 0;
	char buffer[BUF_SIZE];
  char buffer_to_send[BUF_SIZE];
  char file_path[FILE_PATH_SIZE];
  DIR *files;
  //need for get file size
  struct stat mystat;

  struct dirent *myfile;
  while(1){
    nread = read(client_fd, buffer, sizeof(buffer));
  	buffer[nread] = '\0';
    if(strcmp(buffer,"QUIT") == 0){
      break;
    }
    else if(strcmp(buffer, "LIST") == 0 ){
      files = opendir(DIR_NAME);
      while((myfile = readdir(files)) != NULL){
        if(strcmp(myfile->d_name,".") && strcmp(myfile->d_name,"..")){
          sprintf(file_path, "%s/%s",DIR_NAME,myfile->d_name);
          stat(file_path, &mystat);
          sprintf(buffer_to_send, "%zu bytes\t\t%s",mystat.st_size,myfile->d_name);
          write(client_fd,buffer_to_send,sizeof(buffer_to_send));
          sleep(0.1);
        }
      }
      //notify end
      strcpy(buffer_to_send,"#");
      write(client_fd,buffer_to_send,sizeof(buffer_to_send));
      closedir(files);
    }
    else if(strcmp(buffer,"TCP") == 0){
      printf("%s\n", buffer);
    }

  	printf("SERVER: %s\n", buffer);
  	fflush(stdout);
  }
	close(client_fd);
}
