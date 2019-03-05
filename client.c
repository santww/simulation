#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include<pthread.h>

#define BUF_SIZE 1024
#define SERVER_PORT 7000    //Porto do servidor criado na funcao serverThread()

void erro(char *msg);

void * serverThread(void *arg){
  char buffer_receive[BUF_SIZE];
  int fd,client_addr_size,client;
  struct sockaddr_in addr,client_addr;

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(SERVER_PORT);

  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	erro("na funcao socket");
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
	erro("na funcao bind");
  if( listen(fd, 5) < 0)
	erro("na funcao listen");
  client_addr_size = sizeof(client_addr);

  client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
  fcntl(client,F_SETFL,O_NONBLOCK);                                                             //o socket entre o servidor do programa cliente e o cliente do programa servidor e nao bloqueante

  if (client > 0) {
     close(fd);
     while(1){
        if(read(client, buffer_receive, sizeof(buffer_receive)) > 0) //recebe as mensagens da subscricao
            printf("\n%s",buffer_receive);
     }
  }

  close(client);
  pthread_exit(NULL);
}

void * clientThread(char *argv[]){
  char buffer_receive[BUF_SIZE],buffer_send[4][BUF_SIZE];
  char endServer[100];
  int fd,comando,n_read;
  struct sockaddr_in addr;
  struct hostent *hostPtr;
  int i=0;
  pthread_t tid[1];

  strcpy(endServer, argv[1]);                                           //Conecta ao servidor de acordo com os dados fornecidos pelo utilizador
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Nao consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) atoi(argv[2]));

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	erro("socket");
  if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
    erro("Connect");

  if(pthread_create(&tid[i], NULL, serverThread, NULL) != 0 )           //thread para receber as notificacoes referentes a subscricao
    printf("Failed to create thread\n");

  strcpy(buffer_send[0], "0");                                          //primeiro contato entre cliente e servidor
  write(fd, buffer_send, sizeof(buffer_send));

  n_read = read(fd, buffer_receive, sizeof(buffer_receive));
  buffer_receive[n_read] = '\0';
  printf("%s",buffer_receive);

  scanf("%s",buffer_send[1]);
  strcpy(buffer_send[0], "1");
  write(fd, buffer_send, sizeof(buffer_send));

  n_read = read(fd, buffer_receive, sizeof(buffer_receive));

  buffer_receive[n_read] = '\0';
  printf("%s",buffer_receive);

  while(strcmp(buffer_receive,"ID invalido\n") == 0 || strcmp(buffer_receive,"") == 0 ){
    printf("Introduza um id valido: ");
    scanf("%s",buffer_send[1]);
    strcpy(buffer_send[0], "1");
    write(fd, buffer_send, sizeof(buffer_send));

    n_read = read(fd, buffer_receive, sizeof(buffer_receive));
    buffer_receive[n_read] = '\0';
    printf("%s",buffer_receive);
  }

  while(1){
	printf("---------------------------\n1 - Subscricao\n2 - Consultar individual\n3 - Consulta do Grupo\n0 - Fechar o programa\n> Introduza o comando: ");
	scanf("%d",&comando);

	switch(comando){
		case 1: printf("---------------------------\n1 - Subscricao de itens em especificos do utilizador\n2 - Subscricao para a media de grupo\n3 - Subscricao para tudo (Utilizador e Grupo)\n0 - Desativar as notificacoes\nIndique qual item quer subscrever: ");  //Subscricao
                scanf("%s",buffer_send[2]);

                while(strcmp(buffer_send[2],"1") != 0 && strcmp(buffer_send[2],"2") != 0 && strcmp(buffer_send[2],"3") != 0 && strcmp(buffer_send[2],"0") != 0) { //Protecao contra comandos invalidos
                    printf("> Comando invalido! Introduza um comando valido: ");
                    scanf("%s",buffer_send[2]);
                }

                printf("---------------------------\n0 - Todos os Itens\n1 - Activity\n2 - Location\n3 - Calls duration\n4 - Calls made\n5 - Calls missed\n6 - Calls received\n7 - Sms received\n8 - Sms sent\n> Indique o item a subscrever: ");
                scanf("%s",buffer_send[3]);

                while(strcmp(buffer_send[3],"0") != 0 && strcmp(buffer_send[3],"1") != 0 && strcmp(buffer_send[3],"2") != 0 && strcmp(buffer_send[3],"3") != 0 && strcmp(buffer_send[3],"4") != 0 && strcmp(buffer_send[3],"5") != 0 && strcmp(buffer_send[3],"6") != 0 && strcmp(buffer_send[3],"7") != 0 && strcmp(buffer_send[3],"8") != 0) { // Protecao
                    printf("> Comando invalido! Introduza um comando valido: ");
                    scanf("%s",buffer_send[3]);
                }

                strcpy(buffer_send[0], "2");
                write(fd, buffer_send, sizeof(buffer_send));
                break;
		case 2: printf("---------------------------\n0 - Todos os Itens\n1 - Activity\n2 - Location\n3 - Calls duration\n4 - Calls made\n5 - Calls missed\n6 - Calls received\n7 - Department\n8 - Sms received\n9 - Sms sent\n> Indique qual item quer consultar: ");  //Consulta
                scanf("%s",buffer_send[2]);

                while(strcmp(buffer_send[2],"0") != 0 && strcmp(buffer_send[2],"1") != 0 && strcmp(buffer_send[2],"2") != 0 && strcmp(buffer_send[2],"3") != 0 && strcmp(buffer_send[2],"4") != 0 && strcmp(buffer_send[2],"5") != 0 && strcmp(buffer_send[2],"6") != 0 && strcmp(buffer_send[2],"7") != 0 && strcmp(buffer_send[2],"8") != 0 && strcmp(buffer_send[2],"9") != 0 ) { // Protecao
                    printf("> Comando invalido! Introduza um comando valido: ");
                    scanf("%s",buffer_send[2]);
                }

                strcpy(buffer_send[0], "3");
                write(fd, buffer_send, sizeof(buffer_send));
                break;
		case 3: printf("---------------------------\n0 - Todos os Itens\n1 - Activity\n2 - Location\n3 - Calls duration\n4 - Calls made\n5 - Calls missed\n6 - Calls received\n7 - Department\n8 - Sms received\n9 - Sms sent\n> Indique qual item quer consultar: ");
                scanf("%s",buffer_send[2]);

                while(strcmp(buffer_send[2],"0") != 0 && strcmp(buffer_send[2],"1") != 0 && strcmp(buffer_send[2],"2") != 0 && strcmp(buffer_send[2],"3") != 0 && strcmp(buffer_send[2],"4") != 0 && strcmp(buffer_send[2],"5") != 0 && strcmp(buffer_send[2],"6") != 0 && strcmp(buffer_send[2],"7") != 0 && strcmp(buffer_send[2],"8") != 0 && strcmp(buffer_send[2],"9") != 0) { // Protecao
                    printf("> Comando invalido! Introduza um comando valido: ");
                    scanf("%s",buffer_send[2]);
                }

                strcpy(buffer_send[0], "4");
                write(fd, buffer_send, sizeof(buffer_send));
                break;
		case 0: printf("Hasta LA Vista Baby!!!!!\n");
                exit(0);
                break;
		default: printf("Comando Invalido!\n");
                 break;
	}

	if(comando > 0 && comando < 4){ //Imprime a mensagem que recebe do servidor (somente quando executa comando que envia dados para o servidor)
		n_read = read(fd, buffer_receive, sizeof(buffer_receive));
		buffer_receive[n_read] = '\0';
		printf("%s\n",buffer_receive);
	}

  }

  pthread_join(tid[i],NULL);

  close(fd);
  pthread_exit(NULL);
}

int main(int argc, char *argv[]){
  int i = 0;
  pthread_t tid[1];

  if (argc != 3) {
    printf("cliente <host> <port>\n");
    exit(-1);
  }

  if(pthread_create(&tid[i], NULL, clientThread(argv), NULL) != 0 ) //thread para enviar dados ao servidor
    printf("Failed to create thread\n");

  pthread_join(tid[i],NULL);

  return 0;
}

void erro(char *msg) {
	printf("Erro: %s\n", msg);
	exit(-1);
}
