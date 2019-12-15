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
#define FILE_PATH_SIZE 512
#define BUF_SIZE 1024
#define DIR_NAME "server_files"
#define DEBUG

typedef unsigned char byte;

int encrypt(char *target_file, char *source_file, byte* key);
void manage_udp_download();
void manage_tcp(int client_fd);
int fdTCP,fdUDP;

int main(int argc, char *argv[]){
  //Sodium LIB

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
    // if(FD_ISSET(fdUDP,&fdset)){
    //   manage_udp_download();
    // }
  }
}

// void manage_udp_download(){
//   char send_buffer[BUF_SIZE];
//   char buffer[BUF_SIZE];
//   char file_path[FILE_PATH_SIZE];
//   FILE* f;
//   socklen_t size_adr;
//   int nread;
//   struct sockaddr_in client_addr;
//   #ifdef DEBUG
//   printf("UDP\n");
//   #endif
//   size_adr = sizeof(client_addr);
//   nread = recvfrom(fdUDP, buffer, sizeof(buffer), 0,(struct sockaddr*)&client_addr, &size_adr);
//   buffer[nread] = '\0';
//   sprintf(file_path,"server_files/%s",buffer);
//   f = fopen(file_path,"rb");
//   //TODO: verificar se existe no rep
//   if(!f){
//     printf("aaa\n");
//   }
//   else{
//     while((nread=fread(&send_buffer,sizeof(byte),BUF_SIZE,f)) != 0){
//       printf("%d\n", nread);
//       //enviar tudo, o cliente sabe que chegou ao fim quando nao recebe mais bytes
//       sendto(fdUDP, send_buffer, sizeof(byte)*nread, 0, (const struct sockaddr*)&client_addr, sizeof(client_addr));
//       sleep(0.1);
//     }
//   }
//   fclose(f);
//
// }

void manage_tcp(int client_fd){
  unsigned char server_pk[crypto_kx_PUBLICKEYBYTES], server_sk[crypto_kx_SECRETKEYBYTES];
  unsigned char server_rx[crypto_kx_SESSIONKEYBYTES], server_tx[crypto_kx_SESSIONKEYBYTES];
  unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];

  int nread = 0;
	char buffer[BUF_SIZE];
  char buffer_to_send[BUF_SIZE];
  char file_path[FILE_PATH_SIZE];
  char file_name[FILE_PATH_SIZE];
  DIR *files;
  struct dirent *myfile;
  //need for get file size
  struct stat mystat;
  #ifdef DEBUG
  printf("TCP\n");
  #endif

  //Creating keys
  crypto_kx_keypair(server_pk, server_sk);
  read(client_fd, client_pk, sizeof(client_pk));
  write(client_fd, server_pk, sizeof(server_pk));
  if(crypto_kx_server_session_keys(server_rx, server_tx,server_pk, server_sk, client_pk) != 0){
    printf("Suspicious server key\n");
    close(client_fd);
    exit(-1);
  }

  while(1){
    nread = read(client_fd, buffer, sizeof(buffer));
    printf("readdd\n");
  	buffer[nread] = '\0';
    if(strcmp(buffer,"QUIT") == 0){
      break;
    }
    else if(strcmp(buffer, "LIST") == 0 ){
      files = opendir(DIR_NAME);
      while((myfile = readdir(files)) != NULL){
        memset(buffer_to_send, 0, sizeof(buffer_to_send));
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
    else{
      //RECEBE NOME DO FICHEIRO
      FILE* f;
      printf("aaaa\n");
      sprintf(file_path,"server_files/%s",buffer);
      strcpy(file_name,buffer);
      f = fopen(file_path,"rb");
      if(!f){
        //NAO HA FICHIERO
        strcpy(buffer_to_send,"404");
        write(client_fd, buffer_to_send,sizeof(char)*4);
        sleep(0.1);
      }
      else{
        //HA FICHEIRO
        strcpy(buffer_to_send,"OK");
        write(client_fd, buffer_to_send,sizeof(char)*3);
        nread = read(client_fd, buffer, sizeof(buffer));
        //RECEBE UDP OU TCP
        printf("readdd\n");
      	buffer[nread] = '\0';
        if(strcmp(buffer,"UDP-E") == 0 || strcmp(buffer,"UDP-N") == 0){
          int encrypt_file = 0;
          socklen_t size_adr;
          struct sockaddr_in client_addr;
          #ifdef DEBUG
          printf("UDP\n");
          #endif

          if(buffer[4] == 'E'){
            encrypt_file = 1;
          }

          //FIX: para receber o clientaddr nao e necessario fazer isto
          size_adr = sizeof(client_addr);
          nread = recvfrom(fdUDP, buffer, sizeof(buffer), 0,(struct sockaddr*)&client_addr, &size_adr);
          buffer[nread] = '\0';

          if(encrypt_file){
            if (encrypt("server_files/temp",file_path, server_tx) != 0) {
              exit(-1);
            }
            fclose(f);
            f = fopen("server_files/temp","rb");

          }
          //send chunks
          while((nread=fread(&buffer_to_send,sizeof(byte),BUF_SIZE,f)) != 0){
            printf("%d\n", nread);
            //enviar tudo, o cliente sabe que chegou ao fim quando nao recebe mais bytes
            sendto(fdUDP, buffer_to_send, sizeof(byte)*nread, 0, (const struct sockaddr*)&client_addr, sizeof(client_addr));
            sleep(0.1);
          }
          if(encrypt_file){
            remove("server_files/temp");
          }
          fclose(f);
        }
        else if(strcmp(buffer,"TCP-E") == 0 || strcmp(buffer,"TCP-N") == 0){
          int encrypt_file = 0;
          if(buffer[4] == 'E'){
            encrypt_file = 1;
          }

          if(encrypt_file){
            if (encrypt("server_files/temp",file_path, server_tx) != 0) {
              exit(-1);
            }
            fclose(f);
            f = fopen("server_files/temp","rb");
          }

          while((nread=fread(&buffer_to_send,sizeof(byte),BUF_SIZE,f)) != 0){
            printf("%d\n", nread);
            //enviar tudo, o cliente sabe que chegou ao fim quando nao recebe mais bytes
            write(client_fd, buffer_to_send,sizeof(byte)*nread);
            sleep(0.1);
          }

          if(encrypt_file){
            remove("server_files/temp");
          }
          fclose(f);
        }
      }

    }

  	printf("SERVER: %s\n", buffer);
  	fflush(stdout);
  }
}

 int encrypt(char *target_file, char *source_file, byte* key) {

    unsigned char  buf_in[BUF_SIZE];
    unsigned char  buf_out[BUF_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
    unsigned char  header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_state st;
    FILE          *fp_t, *fp_s;
    unsigned long long out_len;
    size_t         rlen;
    int            eof;
    unsigned char  tag;
    fp_s = fopen(source_file, "rb");
    fp_t = fopen(target_file, "wb");
    crypto_secretstream_xchacha20poly1305_init_push(&st, header, key);
    fwrite(header, 1, sizeof header, fp_t);
    do {
        rlen = fread(buf_in, 1, sizeof buf_in, fp_s);
        eof = feof(fp_s);
        tag = eof ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
        crypto_secretstream_xchacha20poly1305_push(&st, buf_out, &out_len, buf_in, rlen, NULL, 0, tag);
        fwrite(buf_out, 1, (size_t) out_len, fp_t);
    } while (! eof);
    fclose(fp_t);
    fclose(fp_s);
    return 0;
}
