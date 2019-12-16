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
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define PORT_LENGTH 10
#define FILE_PATH_SIZE 100
#define ADDR_MAX_SIZE 15



typedef unsigned char byte;

typedef struct connections* ptr_connections;
typedef struct connections{
  pthread_t thread;
  char addr[INET_ADDRSTRLEN];
  int port;
  ptr_connections next;
}Connections;


int fdUDP;
char port[PORT_LENGTH];
char server_addr[INET_ADDRSTRLEN];
int counter;
int save;
pthread_mutex_t mutex_save = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_counter = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ll_connections = PTHREAD_MUTEX_INITIALIZER;
ptr_connections ll_connections = NULL;


void erro(char *msg);
void handle_udp_requests();
void* process_client(void* ptr_fd_client);
void* manage_stdin();
ptr_connections insert_connection(ptr_connections list, ptr_connections new);
ptr_connections delete_connection(ptr_connections list, pthread_t thread);


int main(int argc, char *argv[]) {
  int fd, client;
  struct sockaddr_in addr, client_addr;
  int client_addr_size;
  pthread_t new_thread;
  ptr_connections new_connection;
  memset(server_addr,0,strlen(server_addr));

  mkdir("proxy_files", 0777);

  if (argc != 2) {
    	printf("proxy <port>\nread");
    	exit(-1);
  }

  //Init sodium
  if(sodium_init()<0){
    erro("Couldn't init sodium");
    exit(-1);
  }

  counter = 0;
  save = 0;
  strcpy(port,argv[1]);

  //LIMPA addr
  bzero((void *) &addr, sizeof(addr));
  //Socket para receber requests do cliente
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.2");
  addr.sin_port = htons(atoi(port));

  //CRIA SOCKET UDP
  if ( (fdUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("ERRO");
  if ( bind(fdUDP,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	  printf("ERRO");

  //CRIA SOCKET TCP
  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	 printf("ERRO");
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	 printf("ERRO");
  if( listen(fd, 5) < 0)
  	printf("na funcao listen");

  pthread_create(&new_thread,NULL,manage_stdin,NULL);

  client_addr_size = sizeof(client_addr);

  while (1) {
      // while(waitpid(-1,NULL,WNOHANG)>0);
    client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    if (client > 0) {
        if (pthread_create(&new_thread,NULL,process_client, &client)){
          printf("error creating thread\n");
        }
        new_connection = (ptr_connections)malloc(sizeof(Connections));
        new_connection->thread = new_thread;
        // strcpy(new_connection->addr,inet_ntoa((struct in_addr)client_addr.sin_addr.s_addr));
        inet_ntop( AF_INET, &client_addr, new_connection->addr, INET_ADDRSTRLEN );
        new_connection->port=ntohs(client_addr.sin_port);
        new_connection->next = NULL;
        pthread_mutex_lock(&mutex_ll_connections);
        ll_connections = insert_connection(ll_connections,new_connection);
        pthread_mutex_unlock(&mutex_ll_connections);
      }
  }
  return 0;
}

void erro(char *msg) {
	printf("Erro: %s\nread", msg);
	exit(-1);
}

void* process_client(void* ptr_fd_client){
  //Socket para enviar pacotes ao servidor
  int fd_client = *((int*)ptr_fd_client);
  char endServer[100];
  int fd_server;
  struct sockaddr_in client_addr;
  struct sockaddr_in addr_server;
  struct hostent *hostPtr;
  char buffer[BUFFER_SIZE];
  char buffer_server[BUFFER_SIZE];
  char file_name[FILE_PATH_SIZE];
  int nread;
  unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
  unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
  FILE* f;

  //Le comando passado pelo cliente
  //O primeiro connect -> buffer == ip host
  // nread = read(fd_client, buffer,sizeof(buffer));
  // buffer[nread] = '\0';
  // printf("PROXY : %s\nread", buffer);

  // strcpy(endServer, buffer);
  if ((hostPtr = gethostbyname("127.0.0.1")) == 0)
    	erro("Nao consegui obter endereço");

  //Limpa addr
  bzero((void *) &addr_server, sizeof(addr_server));
  addr_server.sin_family = AF_INET;
  addr_server.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr_server.sin_port = htons((short) atoi(port));
  //endereço do servidor
  if(strlen(server_addr) == 0){
    inet_ntop( AF_INET, &addr_server, server_addr, INET_ADDRSTRLEN );
  }


  //cria socket para proxy<->server
  if((fd_server = socket(AF_INET,SOCK_STREAM,0)) == -1)
	 erro("socket");
  if( connect(fd_server,(struct sockaddr *)&addr_server,sizeof (addr_server)) < 0)
	 erro("Connect");

   //INFO SOBRE CHAVES PUBLICAS
   nread = read(fd_client, client_pk,sizeof(client_pk));
   buffer[nread] = '\0';
   write(fd_server,client_pk,sizeof(client_pk));
   nread = read(fd_server, server_pk,sizeof(server_pk));
   buffer[nread] = '\0';
   write(fd_client,server_pk,sizeof(server_pk));

  while(1){
    nread = read(fd_client, buffer,sizeof(buffer));
    buffer[nread] = '\0';
    printf("COMANDO:%s\n", buffer);
    write(fd_server,buffer,sizeof(buffer));
    if(strcmp(buffer,"QUIT") == 0){
      pthread_mutex_lock(&mutex_ll_connections);
      ll_connections = delete_connection(ll_connections, pthread_self());
      pthread_mutex_unlock(&mutex_ll_connections);
      break;
    }
    else if(strcmp(buffer,"LIST") == 0){
      while(strcmp(buffer_server,"#")){
        nread = read(fd_server,buffer_server,sizeof(buffer_server));
        buffer_server[nread] = '\0';
        printf("%d %s\nread",nread, buffer_server);
        write(fd_client,buffer_server,sizeof(buffer_server));
      }
      //clear buffer from server
      memset(buffer_server,0,strlen(buffer_server));
    }
    else{


      //RECEBE RESPOSTA SOBRE SE O FILE PODE SER DOWNLOADED
      nread = read(fd_server, buffer_server,sizeof(buffer_server));
      buffer_server[nread] = '\0';
      printf("%s\n", buffer_server);
      write(fd_client,buffer_server,sizeof(buffer_server));

      if(strcmp(buffer_server,"OK") == 0){
        //RECEBE PROTOCOLO
        nread = read(fd_client, buffer,sizeof(buffer));
        buffer[nread] = '\0';
        printf("%s\n", buffer);
        write(fd_server,buffer,sizeof(buffer));

        if(save){
          pthread_mutex_unlock(&mutex_counter);
          sprintf(file_name,"proxy_files/%d",counter);
          counter++;
          pthread_mutex_unlock(&mutex_counter);
          f = fopen(file_name,"wb");
        }
        if(strcmp(buffer,"UDP-N") == 0 || strcmp(buffer,"UDP-E") == 0){
          //COMUNICA POR UDP
          socklen_t size_adr;
          size_adr = sizeof(client_addr);
          nread = recvfrom(fdUDP, buffer, sizeof(buffer), 0,(struct sockaddr*)&client_addr, &size_adr);
          buffer[nread] = '\0';
          sendto(fdUDP, buffer,sizeof(byte)*(nread+1),0, (const struct sockaddr*)&addr_server, sizeof(addr_server));

          pthread_mutex_lock(&mutex_save);
          pthread_mutex_unlock(&mutex_save);
          while(1){
            nread = recvfrom(fdUDP, buffer_server, sizeof(byte)*BUFFER_SIZE, 0,NULL, NULL);
            sendto(fdUDP, buffer_server,sizeof(byte)*nread,0, (const struct sockaddr*)&client_addr, sizeof(client_addr));

            pthread_mutex_lock(&mutex_save);
            printf("%d\n", save);
            if(save){
              fwrite(buffer_server,sizeof(byte),nread,f);
            }
            pthread_mutex_unlock(&mutex_save);

            if(nread != BUFFER_SIZE){
              break;
            }
          }
        }
        else{
          while(1){
            nread = read(fd_server, buffer_server, sizeof(buffer_server));

            write(fd_client,buffer_server,sizeof(byte) * nread);

            pthread_mutex_lock(&mutex_save);
            printf("%d\n", save);
            if(save){
              fwrite(buffer_server,sizeof(byte),nread,f);
            }
            pthread_mutex_unlock(&mutex_save);

            if(nread != BUFFER_SIZE){
              pthread_mutex_lock(&mutex_save);
              if(save){
                fclose(f);
              }
              pthread_mutex_unlock(&mutex_save);
              break;
            }
          }
        }
      }
      else if(strcmp(buffer_server,"404") == 0){
        printf("ERRO\n");
      }
    }
  }
  close(fd_server);
}

void* manage_stdin(){
  char buffer[BUFFER_SIZE];
  ptr_connections temp;
  while(1){
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strlen(buffer)-1] = '\0';
    printf("%s\n", buffer);
    if(strcmp(buffer,"SAVE") == 0){
      pthread_mutex_lock(&mutex_save);
      if(save == 0){
        save = 1;
      }
      else{
        save = 0;
      }
      pthread_mutex_unlock(&mutex_save);
    }
    else if(strcmp(buffer,"SHOW") == 0){
      pthread_mutex_lock(&mutex_ll_connections);
      if(ll_connections == NULL){
        printf("\tSEM CLIENTES CONECTADOS\n");
      }
      else{
        temp = ll_connections;
        while(temp){
          printf("\tLIGAÇAO DE: %s PORTO: %d\n", temp->addr,temp->port);
          temp = temp->next;
        }
        printf("\tSERVIDOR: %s PORTO: %s\n", server_addr,port);
      }
      pthread_mutex_unlock(&mutex_ll_connections);
    }
  }
}

ptr_connections insert_connection(ptr_connections list, ptr_connections new){
  ptr_connections temp = list;
  ptr_connections ant = NULL;
  while(temp){
    ant = temp;
    temp = temp->next;
  }
  if(ant){
    ant->next = new;
  }
  else{
    list = new;
  }
  return list;
}

ptr_connections delete_connection(ptr_connections list, pthread_t thread){
  ptr_connections ant = NULL;
  ptr_connections current = list;
  while(current){
    printf("%ld %ld\n", current->thread, thread);
    if(current->thread == thread){
      break;
    }
    ant = current;
    current = current->next;
  }
  if(ant){
    ant->next = current->next;
  }
  else{
    list = current->next;
  }
  free(current);
  return list;
}
