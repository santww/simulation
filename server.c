/*******************************************************************************
 * SERVIDOR no porto 9000, à escuta de novos clientes.  Quando surjem
 * novos clientes os dados por eles enviados são lidos e descarregados no ecran.
 *******************************************************************************/
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
#include <time.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <curl/curl.h>

#define SERVER_PORT     9000
#define BUF_SIZE	1024

/*----------------------------------------------\
|						                        |
|	Estruturas e Funcoes ISABELA		        |
|						                        |
\----------------------------------------------*/

struct string {
	char *ptr;
	size_t len;
};

//Write function to write the payload response in the string structure
size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size*nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

//Initilize the payload string
void init_string(struct string *s) {
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

//Get the Data from the API and return a JSON Object
struct json_object *get_student_data()
{
	struct string s;
	struct json_object *jobj;

	//Intialize the CURL request
	CURL *hnd = curl_easy_init();

	//Initilize the char array (string)
	init_string(&s);

	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	//To run on department network uncomment this request and comment the other
	//curl_easy_setopt(hnd, CURLOPT_URL, "http://10.3.4.75:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");
    //To run from outside
	curl_easy_setopt(hnd, CURLOPT_URL, "http://socialiteorion2.dei.uc.pt:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");

	//Add headers
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "cache-control: no-cache");
	headers = curl_slist_append(headers, "fiware-servicepath: /");
	headers = curl_slist_append(headers, "fiware-service: socialite");

	//Set some options
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc); //Give the write function here
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &s); //Give the char array address here

	//Perform the request
	CURLcode ret = curl_easy_perform(hnd);
	if (ret != CURLE_OK){
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));

		/*jobj will return empty object*/
		jobj = json_tokener_parse(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;

	}
	else if (CURLE_OK == ret) {
		jobj = json_tokener_parse(s.ptr);
		free(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;
	}

}

typedef struct user{        // Estrutura que armazena os dados dos utilizadores do ISABELA
	char id[100],location[20],department[20],activ[20];
	float callsduration,callsmade,callsmissed,callsreceived,smsreceived,smssent;
}User;

User* baseDeDadosIsabela_user();
int diferencaHora(int mAtual,int sAtual,int mAnt,int sAnt);
void comparaDadosU(int client_fd,User usuarioAtual,User usuarioAnt,int subscricao[]);
void comparaDadosM(int client_fd,User usuarioAtual,User usuarioAnt,int subscricao[]);
void comparaDadosUM(int client_fd,User usuariosAtualU,User usuariosAntU,User usuarioAtualM,User usuarioAntM,int subscricao[]);
void primeiro_contacto(int client_fd);
void checkUser(int client_fd,char *user,int tam);
void subscricao(int client_fd);
int retorna_indice(char *user,int tam);
void informacao(int client_fd,char *user,char *comando,int tam);
void process_error(int client_fd);
User calcula_media(int tam);
void media(int client_fd,char *comando,int tam);
void erro(char *msg);

// Variaveis Globais //
User *usuarios;
User m;

int main() {

  int fd,fd_client, client, client_addr_size;
  char endServer[100], buffer[4][BUF_SIZE];
  struct sockaddr_in addr, client_addr,addr_client;
  struct hostent *hostPtr;

  //subscricao
  User subscricaoU,subscricaoM;
  int tabSubsU[] = {0,0,0,0,0,0,0,0};
  int tabSubsM[] = {0,0,0,0,0,0,0,0};
  int tabSubsUM[] = {0,0,0,0,0,0,0,0};
  int sub = 0;                                      //verifica a subscricao - 0: Desativada, 1: Usuario, 2: Media, 3: Usuario e Media - Inicialmente desativada

  int arraylen=0;

  //Get current time
  time_t rawtime;
  struct tm * timeinfo;
  int minuto,segundo;

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );

  segundo = timeinfo->tm_sec;
  minuto = timeinfo->tm_min;

  //Aloca na memoria os usuarios
  usuarios = baseDeDadosIsabela_user();
  //Get array length
  arraylen = json_object_array_length(get_student_data());

  //socket servidor
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

  while (1) {
    //clean finished child processes, avoiding zombies
    //must use WNOHANG or would block whenever a child process was working
    while(waitpid(-1,NULL,WNOHANG)>0);
    //wait for new connection
    client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    fcntl(client,F_SETFL,O_NONBLOCK);                                                                   // socket nao bloqueante

    if (client > 0) {
        if (fork() == 0) {
          close(fd);
          sleep(1); // periodo de tempo em que o programa cliente criar o socket servidor - impede o erro de 'connect'

          //socket cliente
          strcpy(endServer, "127.0.0.1");
          if ((hostPtr = gethostbyname(endServer)) == 0)
            erro("Nao consegui obter endereço");

          bzero((void *) &addr_client, sizeof(addr_client));
          addr_client.sin_family = AF_INET;
          addr_client.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
          addr_client.sin_port = htons(7000);

          if((fd_client = socket(AF_INET,SOCK_STREAM,0)) == -1)
            erro("socket");
          if( connect(fd_client,(struct sockaddr *)&addr_client,sizeof (addr_client)) < 0)
            erro("Connect");

          while(1){
            if(read(client, buffer, sizeof(buffer))>0){
                switch((buffer[0])[0]){ // switch com o primeiro caracter do buffer
                    case '0': primeiro_contacto(client);
                              break;
                    case '1': checkUser(client,buffer[1],arraylen);
                              break;
                    case '2': sub = atoi(buffer[2]);	                                              //indica se a subscricao vai ser do utilizador, da media ou dos dois
                              switch((buffer[2])[0]){
                                case '1': subscricaoU = usuarios[retorna_indice(buffer[1],arraylen)]; //salva Media: O dados e depois compara (em 4 e 4 minutos)
                                          if((buffer[3])[0] == '0'){
                                            for(int i=0;i<8;i++) tabSubsU[i] = 1;
                                          }
                                          else{                                                     // verifica se a subscricao do item esta ativa ou nao
                                             if(tabSubsU[atoi(buffer[3]) - 1] == 0)  				// ativa a subscricao do item
                                                 tabSubsU[atoi(buffer[3]) - 1] = 1;
                                             else									                // desativa a subscricao do item
                                                 tabSubsU[atoi(buffer[3]) - 1] = 0;
                                          }
                                          break;
                                case '2': subscricaoM = calcula_media(arraylen);
                                          if((buffer[3])[0] == '0'){
                                            for(int i=0;i<8;i++) tabSubsM[i] = 1;
                                          }
                                          else{
                                             if(tabSubsM[atoi(buffer[3]) - 1] == 0)
                                                 tabSubsM[atoi(buffer[3]) - 1] = 1;
                                             else
                                                 tabSubsM[atoi(buffer[3]) - 1] = 0;
                                          }
                                          break;
                                case '3': subscricaoU = usuarios[retorna_indice(buffer[1],arraylen)];
                                          subscricaoM = calcula_media(arraylen);
                                          if((buffer[3])[0] == '0'){
                                            for(int i=0;i<8;i++) tabSubsUM[i] = 1;
                                          }
                                          else{
                                             if(tabSubsUM[atoi(buffer[3]) - 1] == 0)
                                                 tabSubsUM[atoi(buffer[3]) - 1] = 1;
                                             else
                                                 tabSubsUM[atoi(buffer[3]) - 1] = 0;
                                          }
                                          break;
                              }

                              minuto = timeinfo->tm_min; //atualiza minuto e segundo para fazer a verificacao dos dados
                              segundo = timeinfo->tm_sec;

                              subscricao(client);

                              break;
                    case '3': informacao(client,buffer[1],buffer[2],arraylen);
                              break;
                    case '4': media(client,buffer[2],arraylen);
                              break;
                }
            }

            time ( &rawtime );			//atualiza o tempo
            timeinfo = localtime ( &rawtime );

            if((sub != 0) && (diferencaHora(timeinfo->tm_min,timeinfo->tm_sec,minuto,segundo) == 15)) { //verifica se os dados foram alterados (subscricao) de 15 em 15 segundos
                switch(sub){
                    case 1: comparaDadosU(fd_client,usuarios[retorna_indice(buffer[1],arraylen)],subscricaoU,tabSubsU);
                            subscricaoU = usuarios[retorna_indice(buffer[1],arraylen)];
                            break;
                    case 2: comparaDadosM(fd_client,calcula_media(arraylen),subscricaoM,tabSubsM);
                            subscricaoM = calcula_media(arraylen);
                            break;
                    case 3: comparaDadosUM(fd_client,usuarios[retorna_indice(buffer[1],arraylen)],subscricaoU,calcula_media(arraylen),subscricaoM,tabSubsUM);
                            subscricaoU = usuarios[retorna_indice(buffer[1],arraylen)];
                            subscricaoM = calcula_media(arraylen);
                            break;
                }

                minuto = timeinfo->tm_min;
                segundo = timeinfo->tm_sec;
            }

            usuarios = baseDeDadosIsabela_user();				        //Atualiza os dados em tempo real
            arraylen = json_object_array_length(get_student_data());
          }
          exit(0);
        }
    }
    close(client);
  }
  return 0;
}

User* baseDeDadosIsabela_user(){
	User *utilizadores;                 // variavel de retorno

	//JSON obect
	struct json_object *jobj_array,*jobj_object;
	int arraylen = 0;
	//Get the student data
	jobj_array = get_student_data();
	//Get array length
	arraylen = json_object_array_length(jobj_array);

	utilizadores = (User *)malloc(arraylen*sizeof(User));       // aloca na memoria de um espaco relativo ao arraylen

	for(int i=0; i<arraylen; i++){
	  if(i!=21){ // Mecanismo de protecao contra o caso dos utilizadores com dados nulos

            jobj_object = json_object_array_get_idx(jobj_array, i);

            strcpy(utilizadores[i].id, json_object_get_string(json_object_object_get(jobj_object, "id")));
            strcpy(utilizadores[i].activ, json_object_get_string(json_object_object_get(jobj_object, "activity")));
            strcpy(utilizadores[i].location, json_object_get_string(json_object_object_get(jobj_object, "location")));
            utilizadores[i].callsduration = atoi(json_object_get_string(json_object_object_get(jobj_object, "calls_duration")));
            utilizadores[i].callsmade = atoi(json_object_get_string(json_object_object_get(jobj_object, "calls_made")));
            utilizadores[i].callsmissed = atoi(json_object_get_string(json_object_object_get(jobj_object, "calls_missed")));
            utilizadores[i].callsreceived = atoi(json_object_get_string(json_object_object_get(jobj_object, "calls_received")));
            strcpy(utilizadores[i].department, json_object_get_string(json_object_object_get(jobj_object, "department")));
            utilizadores[i].smsreceived = atoi(json_object_get_string(json_object_object_get(jobj_object, "sms_received")));
            utilizadores[i].smssent = atoi(json_object_get_string(json_object_object_get(jobj_object, "sms_sent")));

	  }
	}

	return utilizadores;
}

int diferencaHora(int mAtual,int sAtual,int mAnt,int sAnt){
	int res;

    if(mAtual == mAnt){
		res = sAtual - sAnt;
	}
	else{
		res = (sAtual+60) - sAnt;
	}

	return res;
}

void comparaDadosU(int client_fd,User usuarioAtual,User usuarioAnt,int subscricao[]){
	char buffer[BUF_SIZE];

	strcpy(buffer,"");

	for(int i=0;i<8;i++){  // percorre o vetor subscricao
        if(subscricao[i]){ // verifica se o item da tabela esta ativo para subscricao
            switch(i){
                case 0: if(usuarioAtual.activ != usuarioAnt.activ)
                            strcat(buffer,"Usuario: O dado relativo a Activity foi alterado!\n");
                        break;
                case 1: if(strcmp(usuarioAtual.location,usuarioAnt.location) != 0)
                            strcat(buffer,"Usuario: O dado relativo a Location foi alterado!\n");
                        break;
                case 2: if(usuarioAtual.callsduration != usuarioAnt.callsduration)
                            strcat(buffer,"Usuario: O dado relativo a Calls Duration foi alterado!\n");
                        break;
                case 3: if(usuarioAtual.callsmade != usuarioAnt.callsmade)
                            strcat(buffer,"Usuario: O dado relativo a Calls Made foi alterado!\n");
                        break;
                case 4: if(usuarioAtual.callsmissed != usuarioAnt.callsmissed)
                            strcat(buffer,"Usuario: O dado relativo a Calls Missed foi alterado!\n");
                        break;
                case 5: if(usuarioAtual.callsreceived != usuarioAnt.callsreceived)
                            strcat(buffer,"Usuario: O dado relativo a Calls Received foi alterado!\n");
                        break;
                case 6: if(usuarioAtual.smsreceived != usuarioAnt.smsreceived)
                            strcat(buffer,"Usuario: O dado relativo a Sms Received foi alterado!\n");
                        break;
                case 7: if(usuarioAtual.smssent != usuarioAnt.smssent)
                            strcat(buffer,"Usuario: O dado relativo a Sms Sent foi alterado!\n");
                        break;
            }
        }
	}

    if(strcmp(buffer,"") != 0){ // so envia uma notificacao ao cliente se um item for alterado
        printf(">> Mensagem ao Cliente - %s",buffer);
        write(client_fd,buffer,strlen(buffer));
    }

}

void comparaDadosM(int client_fd,User usuarioAtual,User usuarioAnt,int subscricao[]){
	char buffer[BUF_SIZE];

	strcpy(buffer,"");

	for(int i=0;i<8;i++){
        if(subscricao[i]){
            switch(i){
                case 0: if(usuarioAtual.activ != usuarioAnt.activ)
                            strcat(buffer,"Media: O dado relativo a Activity foi alterado!\n");
                        break;
                case 1: if(strcmp(usuarioAtual.location,usuarioAnt.location) != 0)
                            strcat(buffer,"Media: O dado relativo a Location foi alterado!\n");
                        break;
                case 2: if(usuarioAtual.callsduration != usuarioAnt.callsduration)
                            strcat(buffer,"Media: O dado relativo a Calls Duration foi alterado!\n");
                        break;
                case 3: if(usuarioAtual.callsmade != usuarioAnt.callsmade)
                            strcat(buffer,"Media: O dado relativo a Calls Made foi alterado!\n");
                        break;
                case 4: if(usuarioAtual.callsmissed != usuarioAnt.callsmissed)
                            strcat(buffer,"Media: O dado relativo a Calls Missed foi alterado!\n");
                        break;
                case 5: if(usuarioAtual.callsreceived != usuarioAnt.callsreceived)
                            strcat(buffer,"Media: O dado relativo a Calls Received foi alterado!\n");
                        break;
                case 6: if(usuarioAtual.smsreceived != usuarioAnt.smsreceived)
                            strcat(buffer,"Media: O dado relativo a Sms Received foi alterado!\n");
                        break;
                case 7: if(usuarioAtual.smssent != usuarioAnt.smssent)
                            strcat(buffer,"Media: O dado relativo a Sms Sent foi alterado!\n");
                        break;
            }
        }
    }

 	if(strcmp(buffer,"") != 0){
        printf(">> Mensagem ao Cliente - %s",buffer);
        write(client_fd,buffer,strlen(buffer));
    }

}

void comparaDadosUM(int client_fd,User usuariosAtualU,User usuariosAntU,User usuarioAtualM,User usuarioAntM,int subscricao[]){
	char buffer[BUF_SIZE];

	strcpy(buffer,"");

	for(int i=0;i<8;i++){
        if(subscricao[i]){
            switch(i){
                case 0: if(usuariosAtualU.activ != usuariosAntU.activ)   			                //USUARIO
                            strcat(buffer,"Usuario: O dado relativo a Activity foi alterado!\n");

                        if(usuarioAtualM.activ != usuarioAntM.activ)				                //MEDIA
                            strcat(buffer,"Media: O dado relativo a Activity foi alterado!\n");

                        break;
                case 1: if(usuariosAtualU.callsduration != usuariosAntU.callsduration)
                            strcat(buffer,"Usuario: O dado relativo a Calls Duration foi alterado!\n");

                        if(usuarioAtualM.callsduration != usuarioAntM.callsduration)
                            strcat(buffer,"Media: O dado relativo a Calls Duration foi alterado!\n");

                        break;
                case 2: if(usuariosAtualU.callsmade != usuariosAntU.callsmade)
                            strcat(buffer,"Usuario: O dado relativo a Calls Made foi alterado!\n");

                        if(usuarioAtualM.callsmade != usuarioAntM.callsmade)
                            strcat(buffer,"Media: O dado relativo a Calls Made foi alterado!\n");

                        break;
                case 3: if(usuariosAtualU.callsmissed != usuariosAntU.callsmissed)
                            strcat(buffer,"Usuario: O dado relativo a Calls Missed foi alterado!\n");

                        if(usuarioAtualM.callsmissed != usuarioAntM.callsmissed)
                            strcat(buffer,"Media: O dado relativo a Calls Missed foi alterado!\n");

                        break;
                case 4: if(usuariosAtualU.callsreceived != usuariosAntU.callsreceived)
                            strcat(buffer,"Usuario: O dado relativo a Calls Received foi alterado!\n");

                        if(usuarioAtualM.callsreceived != usuarioAntM.callsreceived)
                            strcat(buffer,"Media: O dado relativo a Calls Received foi alterado!\n");

                        break;
                case 5: if(usuariosAtualU.smsreceived != usuariosAntU.smsreceived)
                            strcat(buffer,"Usuario: O dado relativo a Sms Received foi alterado!\n");

                        if(usuarioAtualM.smsreceived != usuarioAntM.smsreceived)
                            strcat(buffer,"Media: O dado relativo a Sms Received foi alterado!\n");

                        break;
                case 6: if(usuariosAtualU.smssent != usuariosAntU.smssent)
                            strcat(buffer,"Usuario: O dado relativo a Sms Sent foi alterado!\n");

                        if(usuarioAtualM.smssent != usuarioAntM.smssent)
                            strcat(buffer,"Media: O dado relativo a Sms Sent foi alterado!\n");

                        break;
                case 7: if(strcmp(usuariosAtualU.location,usuariosAntU.location) != 0)
                            strcat(buffer,"Usuario: O dado relativo a Location foi alterado!\n");

                        if(strcmp(usuarioAtualM.location,usuarioAntM.location) != 0)
                            strcat(buffer,"Media: O dado relativo a Location foi alterado!\n");

                        break;
            }
        }
	}

	if(strcmp(buffer,"") != 0){
        printf(">> Mensagem ao Cliente - %s",buffer);
        write(client_fd,buffer,strlen(buffer));
    }

}

void primeiro_contacto(int client_fd){
	char buffer[BUF_SIZE];

	snprintf(buffer,sizeof(buffer),"Bem vindo!\n> Insira o teu id:\n");
	printf(">> Mensagem ao Cliente - \n%s",buffer);

	write(client_fd,buffer,strlen(buffer));
}

void subscricao(int client_fd){
	char buffer[BUF_SIZE];

	snprintf(buffer,sizeof(buffer),"Subscricao feita com sucesso!\n");
	printf(">> Mensagem ao Cliente - %s",buffer);

	write(client_fd,buffer,strlen(buffer));
}

void checkUser(int client_fd,char *user,int tam){
	char buffer[BUF_SIZE];
	int verif=0;

	for(int i=1;i<tam;i++){
		if(strcmp(usuarios[i].id,user) == 0)
			verif=1;
	}

	if(verif == 1)                                         // se encontrou o ID na base de dados
		snprintf(buffer,sizeof(buffer),"ID valido\n");
	else
		snprintf(buffer,sizeof(buffer),"ID invalido\n");

	printf(">> Mensagem ao Cliente - %s",buffer);
	write(client_fd,buffer,strlen(buffer));
}

User calcula_media(int tam){
	User res;
	int location[] = {0,0,0};
	int activity[] = {0,0,0,0,0,0,0,0};
	int indice, maior;

	res.callsduration=0; res.callsmade=0; res.callsmissed=0; res.callsreceived=0; res.smssent=0; res.smsreceived = 0;



	for(int i=0;i<tam;i++){
	   if(i!=21){ // Pula o ID com os dados nulos
       		//soma os valores
            res.callsduration = res.callsduration + usuarios[i].callsduration;
            res.callsmade = res.callsmade + usuarios[i].callsmade;
            res.callsmissed = res.callsmissed + usuarios[i].callsmissed;
            res.callsreceived = res.callsreceived + usuarios[i].callsreceived;
            res.smsreceived = res.smsreceived + usuarios[i].smsreceived;
            res.smssent = res.smssent + usuarios[i].smssent;

            //compara com todas as localidades possiveis
            if(strcmp(usuarios[i].location,"University") == 0)
                location[0]++;
            if(strcmp(usuarios[i].location,"House") == 0)
                location[1]++;
            if(strcmp(usuarios[i].location,"Other") == 0)
                location[2]++;

            //compara com todas as atividades possiveis
            if(strcmp(usuarios[i].activ,"Exercise") == 0)
                activity[0]++;
            if(strcmp(usuarios[i].activ,"Sleeping") == 0)
                activity[1]++;
            if(strcmp(usuarios[i].activ,"Classes") == 0)
                activity[2]++;
            if(strcmp(usuarios[i].activ,"Tilting") == 0)
                activity[3]++;
            if(strcmp(usuarios[i].activ,"Walking") == 0)
                activity[4]++;
            if(strcmp(usuarios[i].activ,"In vehicle") == 0)
                activity[5]++;
            if(strcmp(usuarios[i].activ,"Unknown") == 0)
                activity[6]++;
            if(strcmp(usuarios[i].activ,"Still") == 0)
                activity[7]++;
	   }
	}

	//Media da localizacao
	maior = location[0];
	indice = 0;
    for(int j=0;j<3;j++){
        if(location[j] > maior){
			maior = location[j];
			indice = j;
		}
	}
	switch(indice){
		case 0: strcpy(res.location, "University");
                break;
		case 1: strcpy(res.location, "House");
                break;
		case 2:	strcpy(res.location, "Other");
                break;
	}

	//Media da Atividade
	maior = activity[0];
	indice = 0;
	for(int k=0;k<8;k++){
		if(activity[k] > maior){
			maior = activity[k];
			indice = k;
		}
	}
	switch(indice){
		case 0: strcpy(res.activ, "Exercise");
                break;
		case 1: strcpy(res.activ, "Sleeping");
                break;
		case 2:	strcpy(res.activ, "Classes");
                break;
		case 3:	strcpy(res.activ, "Tilting");
                break;
		case 4:	strcpy(res.activ, "Walking");
                break;
		case 5:	strcpy(res.activ, "In vehicle");
                break;
		case 6:	strcpy(res.activ, "Unknown");
                break;
		case 7:	strcpy(res.activ, "Still");
                break;
	}

	res.callsduration = res.callsduration/tam;
	res.callsmade = res.callsmade/tam;
	res.callsmissed = res.callsmissed/tam;
	res.callsreceived = res.callsreceived/tam;
	res.smsreceived = res.smsreceived/tam;
	res.smssent = res.smssent/tam;

    snprintf(res.department,sizeof(res.department),"DEI-IRC");

	return res;
}

void media(int client_fd,char *comando,int tam){
    char buffer[BUF_SIZE];
	User res = calcula_media(tam);

	switch(comando[0]){
		case '0': snprintf(buffer,sizeof(buffer),"Activity: %s\nLocation: %s\nCalls duration: %.2f\nCalls made: %.2f\nCalls missed: %.2f\nCalls received: %.2f\nDepartment: %s\nSms received: %.2f\nSms sent: %.2f\n",res.activ,res.location,res.callsduration,res.callsmade,res.callsmissed,res.callsreceived,res.department,res.smsreceived,res.smssent);
			      break;
		case '1': snprintf(buffer,sizeof(buffer),"Activity: %s\n",res.activ);
			      break;
		case '2': snprintf(buffer,sizeof(buffer),"Location: %s\n",res.location);
			      break;
		case '3': snprintf(buffer,sizeof(buffer),"Calls duration: %.2f\n",res.callsduration);
                  break;
		case '4': snprintf(buffer,sizeof(buffer),"Calls made: %.2f\n",res.callsmade);
			      break;
		case '5': snprintf(buffer,sizeof(buffer),"Calls missed: %.2f\n",res.callsmissed);
			      break;
		case '6': snprintf(buffer,sizeof(buffer),"Calls received: %.2f\n",res.callsreceived);
			      break;
		case '7': snprintf(buffer,sizeof(buffer),"Department: %s\n",res.department);
                  break;
		case '8': snprintf(buffer,sizeof(buffer),"Sms received: %.2f\n",res.smsreceived);
			      break;
		case '9': snprintf(buffer,sizeof(buffer),"Sms sent: %.2f\n",res.smssent);
			      break;
	}

    printf(">> Mensagem ao Cliente -\n%s",buffer);
	write(client_fd,buffer,strlen(buffer));
}

int retorna_indice(char *user,int tam){
	int res;

	for(int i=0;i<tam;i++){
		if(strcmp(usuarios[i].id,user) == 0){
			res=i;
			break;                                  //se encontrar o indice do ID nao precisa mais percorrer a variavel
		}
	}

	return res;
}

void informacao(int client_fd,char *user,char *comando,int tam){
	char buffer[BUF_SIZE];
	int i = retorna_indice(user,tam);

	switch(comando[0]){
  		case '0': snprintf(buffer,sizeof(buffer),"Activity: %s\nLocation: %s\nCalls duration: %.2f\nCalls made: %.2f\nCalls missed: %.2f\nCalls received: %.2f\nDepartment: %s\nSms received: %.2f\nSms sent: %.2f\n",usuarios[i].activ,usuarios[i].location,usuarios[i].callsduration,usuarios[i].callsmade,usuarios[i].callsmissed,usuarios[i].callsreceived,usuarios[i].department,usuarios[i].smsreceived,usuarios[i].smssent);
                  break;
  		case '1': snprintf(buffer,sizeof(buffer),"Activity: %s\n",usuarios[i].activ);
                  break;
		case '2': snprintf(buffer,sizeof(buffer),"Location: %s\n",usuarios[i].location);
                  break;
		case '3': snprintf(buffer,sizeof(buffer),"Calls duration: %.2f\n",usuarios[i].callsduration);
                  break;
		case '4': snprintf(buffer,sizeof(buffer),"Calls made: %.2f\n",usuarios[i].callsmade);
			      break;
		case '5': snprintf(buffer,sizeof(buffer),"Calls missed: %.2f\n",usuarios[i].callsmissed);
                  break;
		case '6': snprintf(buffer,sizeof(buffer),"Calls received: %.2f\n",usuarios[i].callsreceived);
                  break;
		case '7': snprintf(buffer,sizeof(buffer),"Department: %s\n",usuarios[i].department);
                  break;
		case '8': snprintf(buffer,sizeof(buffer),"Sms received: %.2f\n",usuarios[i].smsreceived);
			      break;
		case '9': snprintf(buffer,sizeof(buffer),"Sms sent: %.2f\n",usuarios[i].smssent);
                  break;
	}

	printf(">> Mensagem ao Cliente -\n%s",buffer);
	write(client_fd,buffer,strlen(buffer));
}

void process_error(int client_fd){
	char buffer[BUF_SIZE];

	snprintf(buffer,sizeof(buffer),"Erro! O servidor ainda nao recebeu numeros! \n");
	printf("Mensagem ao Cliente: %s",buffer);
	write(client_fd,buffer,strlen(buffer));
}

void erro(char *msg){
	printf("Erro: %s\n", msg);
	exit(-1);
}
