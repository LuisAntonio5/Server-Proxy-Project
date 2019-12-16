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
#include <time.h>
#include <sodium.h>
#include <sys/stat.h>
#include <sys/types.h>


//get files from ./server_files
#include <dirent.h>
#include <sys/stat.h>

#define SERVER_PORT     9000
#define BUF_SIZE 1024

typedef unsigned char byte;


byte checksum(byte* stream,size_t size);
void erro(char *msg);
int decrypt(char *target_file, char *source_file, byte* key);

int main(int argc, char *argv[]){
  //SODIUM LIB
  unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
  unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];
  unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];

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
  time_t start_d, end_d;
  int fail_download;

  //Init sodium
  if(sodium_init()<0){
    erro("Couldn't init sodium");
    exit(-1);
  }

  mkdir("client_files", 0777);

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    	printf("Nao consegui obter endereço\n");

  //LIMPA addr
  bzero((void *) &server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  server_addr.sin_port = htons(atoi(argv[3]));
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
  //ESTABELCE LIGAÇAO
  // printf("%s\n", argv[2]);
  // write(fdTCP, argv[2], 1+sizeof(argv[2]));
  // sleep(1);
  //share public keys
  crypto_kx_keypair(client_pk, client_sk);
  write(fdTCP, client_pk, sizeof(client_pk));
  read(fdTCP, server_pk, sizeof(server_pk));

  //RECEBER INFO SOBRE REJEITADO
  nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
  buffer_to_rcv[nread] = '\0';
  printf("%s\n",buffer_to_rcv );
  if(strcmp(buffer_to_rcv,"OK") == 0){
    printf("Connectando ao servidor..\n");
  }
  else if(strcmp(buffer_to_rcv,"QUIT") == 0){
    printf("Limite ultrapassado\n");
    strcpy(buffer,"QUIT");
    write(fdTCP, buffer, sizeof(buffer));
    return 0;
  }
  if(crypto_kx_client_session_keys(client_rx, client_tx,client_pk, client_sk, server_pk) != 0){
    printf("Suspicious server key\n");
    close(fdTCP);
    exit(-1);
  }
  while(1){
    command_lenght = 0;
    fail_download = 0;
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
          FILE* f;
          int total_bytes = 0;
          fd_set fdset;
          //write(fdTCP, "UDP", 5*sizeof(char));
          //verify if file exists
          write(fdTCP, split_command[3], sizeof(split_command[3]));

          // wait to check if file was found
          nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
          buffer_to_rcv[nread] = '\0';
          printf("%s\n", buffer_to_rcv);
          if(strcmp(buffer_to_rcv,"OK") == 0){
            if(strcmp(split_command[2],"ENC") == 0){
              strcpy(buffer,"UDP-E");
            }
            else{
              strcpy(buffer,"UDP-N");
            }
            write(fdTCP, buffer,sizeof(buffer));
            struct timeval tv = {2,0};
            sendto(fdUDP, (const char*)split_command[3], strlen(split_command[3]), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));

            if(strcmp(split_command[2],"ENC") == 0){
              f = fopen("temp","wb");
            }
            else{
              f = fopen(split_command[3],"wb");
            }

            FD_ZERO(&fdset);

            //sai quando não existir nada para receber
            time(&start_d);
            while(1){
              FD_SET(fdUDP,&fdset);
              int not_empty = select(fdUDP+1,&fdset,NULL,NULL,&tv);
              printf("EMPTY %d\n", not_empty);
              if(not_empty){
                nread = recvfrom(fdUDP, buffer_to_rcv, sizeof(byte)*BUF_SIZE, 0,NULL, NULL);
                if(checksum(buffer_to_rcv,nread) != 0){
                  fail_download = 1;
                }
                printf("%d\n", nread);
                fwrite(buffer_to_rcv,sizeof(byte),nread-1,f);
                total_bytes = total_bytes + nread;
              }
              else{
                break;
              }
            }
            if(fail_download == 1){
              printf("\tERRO NO DOWNLOAD. DOWNLOAD NAO VALIDO\n");
            }
            fclose(f);

            if(strcmp(split_command[2],"ENC") == 0){
              decrypt(split_command[3], "temp",client_rx);
              remove("temp");
            }

            time(&end_d);
            double time_taken = (double)(end_d - start_d);
            printf("\t\tDOWNLOAD %s completed\n", split_command[3]);
            printf("\t\t%d bytes received\n", total_bytes);
            printf("\t\t%s used\n", split_command[1]);
            printf("\t\tTEMPO %f seconds\n",time_taken);
          }
          else{
            printf("Received a File not found error.\n");
          }
        }

        else if(strcmp(split_command[0],"DOWNLOAD") == 0 && strcmp(split_command[1],"TCP") == 0){
          int total_bytes = 0;
          write(fdTCP, split_command[3], sizeof(split_command[3]));

          // wait to check if file was found
          nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
          buffer_to_rcv[nread] = '\0';
          printf("%s\n", buffer_to_rcv);
          if(strcmp(buffer_to_rcv,"OK") == 0){
            FILE* f;

            //SEND INFO ABOUT PROTOCOL AND DATA ENC
            if(strcmp(split_command[2],"ENC") == 0){
              strcpy(buffer,"TCP-E");
              f = fopen("temp","wb");
            }
            else{
              strcpy(buffer,"TCP-N");
              f = fopen(split_command[3],"wb");
            }
            write(fdTCP, buffer,sizeof(buffer));

            //START DOWNLOAD
            time(&start_d);
            while(1){
              nread = read(fdTCP, buffer_to_rcv, sizeof(buffer_to_rcv));
              if(checksum(buffer_to_rcv,nread) != 0){
                fail_download = 1;
              }
              fwrite(buffer_to_rcv,sizeof(byte),nread-1,f);
              total_bytes = total_bytes + nread;
              if(nread != BUF_SIZE){
                break;
              }
            }

            fclose(f);
            if(strcmp(split_command[2],"ENC") == 0){
              decrypt(split_command[3], "temp",client_rx);
              remove("temp");
            }

            //DOWNLOAD COMPLETED
            if(fail_download == 1){
              printf("\tERRO NO DOWNLOAD. DOWNLOAD NAO VALIDO\n");
            }
            time(&end_d);
            double time_taken = (double)(end_d - start_d);
            printf("\t\tDOWNLOAD %s completed\n", split_command[3]);
            printf("\t\t%d bytes received\n", total_bytes);
            printf("\t\t%s used\n", split_command[1]);
            printf("\t\tTEMPO %f seconds\n",time_taken);
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

 int decrypt(char *target_file, char *source_file, byte* key)
{
    byte  buf_in[BUF_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
    byte  buf_out[BUF_SIZE];
    byte  header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_state st;
    FILE          *fp_t, *fp_s;
    unsigned long long out_len;
    size_t         rlen;
    int            eof;
    unsigned char  tag;
    fp_s = fopen(source_file, "rb");
    fp_t = fopen(target_file, "wb");
    fread(header, 1, sizeof header, fp_s);
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 0) {
        /* incomplete header */
        fclose(fp_t);
        fclose(fp_s);
        return -1;
    }
    do {
        rlen = fread(buf_in, 1, sizeof buf_in, fp_s);
        eof = feof(fp_s);
        if (crypto_secretstream_xchacha20poly1305_pull(&st, buf_out, &out_len, &tag, buf_in, rlen, NULL, 0) != 0) {
            /* corrupted chunk */
            fclose(fp_t);
            fclose(fp_s);
            return -1;
        }
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL && ! eof) {
            /* premature end (end of file reached before the end of the stream) */
            fclose(fp_t);
            fclose(fp_s);
            return -1;
        }
        fwrite(buf_out, 1, (size_t) out_len, fp_t);
    } while (! eof);

    fclose(fp_t);
    fclose(fp_s);
    return 0;
}

byte checksum(byte* stream,size_t size){
  byte chk = 0;
  while(size-- != 0){
    chk -= *stream++;
  }
  return chk;
}
