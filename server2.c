/* 
   gcc -lpthread server2.c -o server

protocol definition:

client request:                                               server answer:

1           - hash method                                     method
2           - hash to decode                                  hash
3           - wordlist id                                     identifier to wordlist
4           - wordlist size                                   wordlist size
5           - wordlist request                                wordlist
6           - prossessing                                     nothing
7|id|hash   - not found in this id                            another wordlist id; if == -1 job is ended
8|id|index  - pass found wordlist = id and position = index   server don't answer

*/
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h> //kill
#include <pthread.h> //threads
#include <unistd.h> //sleep
#include <time.h>

//includes for the warnings solve:
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_CLIENT 1000
#define MAX_WORDLIST_FRAGMENTS 1000000
#define MAX_CHAR_WORDLISTRAW 100000000

struct wordlistFragment{
	int id;
	int idClient;
	unsigned long int firstindex;
	int status;
};

struct clientStruct{
	int id;
	int sock;
	int idWordlist;
	int wordlistSize;
	char * wordlistRaw;
};

struct wordlistFragment * wordlistpool[MAX_WORDLIST_FRAGMENTS];
struct clientStruct * clientpool[MAX_CLIENT];
int nline = 100000; // lines number to divide the wordlist
unsigned int currentIndexWordlist = 0;
unsigned long int currentIdWordlist = 0;
unsigned long int currentIdClient = 0;
int verbose = 1;
int status = 1;
int sockfd;
int hashtobreak = 1;

char good[] = "\e[01;34m[\e[00m+\e[01;34m]\e[00m"; //Ighor's idea github.com/f0rb1dd3n/
char bad[] = "\e[01;31m[\e[00m-\e[01;31m]\e[00m";  //Ighor's idea github.com/f0rb1dd3n/
char warn[] = "\e[01;33m[\e[00m!\e[01;33m]\e[00m"; //Ighor's idea github.com/f0rb1dd3n/

char hashtosend [] = "0567d2132f50b437f34bfd172dfa003b1081fa72";

char * prepareWordlistToSend(char ** wordlist);
void *listenAnswerSock (void *client); /* function prototype */
void answerClient (struct clientStruct * clientStruct, char * buffer);
char ** cutWordlist(char * wordlistRaw);
void * openFile();

pthread_mutex_t lock;

void error(char *msg)
{
	perror(msg);
	exit(1);
}

//variables for protocol:
int method = 1;
int hashsize = 1024;
char hashQuery;
int wordlistSize;
char * wordlistRaw = 0;
char ** wordlist = 0;

clock_t start;
clock_t end;

int main(int argc, char *argv[])
{
	int portno, clilen, pid;
	struct sockaddr_in serv_addr, cli_addr;
	int newsockfd;
	struct clientStruct * clientStore = 0;

	if (pthread_mutex_init(&lock, NULL) != 0){
		printf("\n mutex init failed\n");
		return 1;
	}

	pthread_t tidOpen;
	int t = pthread_create(&tidOpen, NULL, openFile, NULL);
	if (t != 0){
		close (sockfd);
		error("ERROR on thread creation");
	}
	
	if (argc < 2) {
		 fprintf(stderr,"ERROR, no port provided\n");
		 exit(1);
	}
    if (argc > 2)
    	verbose = atoi(argv[2]);

    if ( verbose )
    	printf("%s waiting archive is open\n", warn);
	pthread_join(tidOpen, NULL); //waiting the thread finished
    printf("hash to decode: ");
    scanf("%s", hashtosend);


    
    wordlist = cutWordlist(wordlistRaw); 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
				      sizeof(serv_addr)) < 0) 
				      error("ERROR on binding");
	listen(sockfd, MAX_CLIENT);
	clilen = sizeof(cli_addr);
	
	time(&start);
	while (status) {
		if( verbose )
			printf("%s waiting a new client connection\n", good);
		newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) {
			close(sockfd);
			error("ERROR on accept");
		}
		printf("%s Connection established! Client id: %d\n", good, currentIdClient);
		clientStore = malloc(sizeof(struct clientStruct));
		clientStore->id = currentIdClient++;
		clientStore->sock = newsockfd;
		pthread_mutex_lock(&lock);
		clientpool[clientStore->id] = clientStore;
		pthread_mutex_unlock(&lock);
		pthread_t tid;
		int t = pthread_create(&tid, NULL, listenAnswerSock, clientStore);
		if (t != 0){
			close (sockfd);
			error("ERROR on thread creation");
		}
	} /* end of while */
	time(&end);
	double seconds = difftime (end, start);
	printf("\nIt spended %lf secconds\n", seconds);
	close(sockfd);   
	pthread_mutex_destroy(&lock);
	return 0; /* we never get here */
}

/******** listenAnswerSock() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void *listenAnswerSock (void *clientStruct){
	struct clientStruct * client = (struct clientStruct *) clientStruct;
	int t, requestClient;
	int sockno = client->sock;
	int cliId = client->id;
	pthread_t tid;
	int msgsz = 1024;
	char * buffer = 0;
	buffer = malloc(sizeof(char) * msgsz);

	while ( hashtobreak ){
		//listening
		if (verbose)
			printf("listening client %d...\n", cliId);

		bzero(buffer, msgsz);
		if( verbose > 2 )
			printf("waiting client %d\n", cliId);
		//sleep(1);
		int n = read(sockno, buffer, msgsz);
		if (n < 0){
			printf("%s socket %d: ERROR reading to socket client %d\n",warn, sockno, cliId);
			break;
		} 
		if(n == 0){
			printf("%s socket %d: Connection is close\n",warn, sockno);
			break;
		} 
		if( verbose > 2 )
			printf("client %d sent: %s\n", cliId, buffer);
		
		answerClient(client, buffer);
	}

	if ( buffer ){
		free (buffer);
		buffer = 0;
	}

	if ( verbose )
		printf("%s client %d is gone\n",warn, cliId);
	if(!hashtobreak){
		pthread_mutex_lock(&lock);
		status = 0;
		pthread_mutex_unlock(&lock);
	}

	close (sockno);
	free (client);
	client = 0;
	return NULL;
}

void answerClient (struct clientStruct * client, char * bufferR){
	int cliId = client->id;
	if ( verbose > 2 )
		printf("Client %d Message received: %s\n", cliId, bufferR);
	int i = 0;
	char x [1024];
	bzero(x, 1024);
	while(bufferR[i] != '\0'){
		if (bufferR[i++] == '|')
			break;
	}
	size_t n = i;
	strncat(x, bufferR, n);
	int requestClient = atoi( x );
	char * buffer = 0;
	int sz;
	int sock = client->sock;
	struct wordlistFragment * wordlistpiece = 0;
	if ( requestClient == 1 ){//hash method
		sz = 8;
		buffer = malloc (sizeof(char) * sz);
		bzero(buffer, sz);
		sprintf(buffer, "%d", method);	
	}

	if ( requestClient == 2 ){// hash
		sz = hashsize;
		buffer = malloc ( sizeof(char) * sz );
		bzero(buffer, sz);
		strcpy ( buffer, hashtosend );
	}

	if ( requestClient == 3 ){// wordlist id | prepare a wordlist to this client
		sz = 16;
		wordlistpiece = malloc(sizeof(struct wordlistFragment));
		wordlistpiece->status = 1;
		wordlistpiece->idClient = cliId;
		pthread_mutex_lock(&lock);
		wordlistpiece->id = currentIdWordlist++;
		wordlistpool[wordlistpiece->id] = wordlistpiece;
		client->idWordlist = wordlistpiece->id;
		wordlistpiece->firstindex = currentIndexWordlist;
		client->wordlistRaw = prepareWordlistToSend(wordlist); //wordlistraw cleaned: buffer in request=5
		pthread_mutex_unlock(&lock);
		client->wordlistSize = (int) strlen(client->wordlistRaw);
		buffer = malloc (sizeof(char) * sz);
		bzero(buffer, sz);
		sprintf(buffer, "%d", wordlistpiece->id);
		wordlistpiece = 0;
		
	}
	if ( requestClient == 4 ){// wordlist size
		sz = 16;
		buffer = malloc (sizeof(char) * sz);
		bzero(buffer, sz);
		int len = client->wordlistSize;
		sprintf(buffer, "%d", len);
	}
	if ( requestClient == 5 ){// wordlist
		sz = client->wordlistSize;
		buffer = client->wordlistRaw;
		if ( verbose > 2 )
			printf("Buffer real size: %d\n", strlen(buffer));
		//buffer = malloc(sizeof(char) * sz);
		//bzero(buffer, sz);
		//strcpy(buffer, client->wordlistRaw);
	}
	if ( requestClient == 7 ){// another wordlist size; if == 0 job is ended
		
		//getting wordlist id
		char idwordlistaux[16];
		bzero (idwordlistaux, 16);
		int j = i;
		for ( i = j; i < 1024; i++ ){
			if ( bufferR[i] == '|' ){
				break;
			}
		}
		size_t n = i - j;
		strncat(idwordlistaux, &bufferR[j], n);
		int wordlistId = atoi (idwordlistaux);
		if ( wordlistId == client->idWordlist ){
			
			//getting hash
			char hashaux[1024];
			bzero(hashaux, 1024);
			strcat (hashaux, &bufferR[i+1]);

				//todo verificar se esta hash ja foi finalizada
			if(hashtobreak != 0){
				
				if( currentIndexWordlist < wordlistSize ){
					pthread_mutex_lock(&lock);
					wordlistpiece = wordlistpool[client->idWordlist];
					wordlistpiece->status = 0; //not found
					wordlistpiece = 0;
					wordlistpiece = malloc(sizeof(struct wordlistFragment));
					wordlistpiece->idClient = client->id;
					wordlistpiece->status = 1;
					
					wordlistpiece->id = currentIdWordlist++;
					wordlistpool[wordlistpiece->id] = wordlistpiece;
					client->idWordlist = wordlistpiece->id;
					wordlistpiece->firstindex = currentIndexWordlist;
					client->wordlistRaw = prepareWordlistToSend(wordlist);
					pthread_mutex_unlock(&lock);
					client->wordlistSize = (int) strlen(client->wordlistRaw);
					sz = 16;
					buffer = malloc (sizeof(char) * sz);
					sprintf(buffer, "%d", wordlistpiece->id);
					if(verbose > 2)
						printf("wordlistid answer: %s\n", buffer);
					wordlistpiece = 0;
					
				}else{
					sz = 16;
					buffer = malloc (sizeof(char) * sz);
					strcpy(buffer, "-1");
				}
			}else{
				status = 0;
				printf("Hash to send don't found!\n");
				return;
			}
		}
		
	}
	if ( requestClient == 8 ){// don't answer
		//getting wordlist id
		char idwordlistaux[16];
		bzero (idwordlistaux, 16);
		int j = i;
		for ( i = j; i < 1024; i++ ){
			if ( bufferR[i] == '|' ){
				break;
			}
		}
		size_t n = i - j;
		strncat(idwordlistaux, &bufferR[j], n);
		int wordlistId = atoi (idwordlistaux);

		if ( wordlistId == client->idWordlist ){
			//getting position
			char positionAux[1024];
			bzero (positionAux, 1023);
			strcat (positionAux, &bufferR[i+1]);
			pthread_mutex_lock(&lock);
			wordlistpiece = wordlistpool[client->idWordlist];
			pthread_mutex_unlock(&lock);
			int position = atoi ( positionAux );
			wordlistpiece->status = 2; //found here
			printf("Client %d found the password postiion %d\npassword is: %s\n", 
												client->id, 
												position, 
												wordlist[position+wordlistpiece->firstindex]);
		}
		pthread_mutex_lock(&lock);
		hashtobreak--;
		pthread_mutex_unlock(&lock);
		return;
	}

	if( buffer ){

		if (verbose)
			printf("Answer a client %d request: %d...\n", cliId, requestClient);
		if (verbose > 3 )
			printf("sock: %d buffer: %s sz: %d\n", sock, buffer, sz);

		int n = write(sock, buffer, sz);

		if ( n < 0 ){
			printf("%s socket %d: ERROR writing from socket\n",bad, sock);
		} 
		if ( n == 0 ){
			printf("%s socket %d: Connection is close\n",warn ,sock);
		} 
		
		free ( buffer );
		buffer = 0;
		
		if ( verbose )
			printf("%s client %d request %d answered!\n", good, cliId, requestClient);
		}
	return;
}

char ** cutWordlist(char * wordlistRaw){
	if ( verbose > 1 )
		printf("%s cutting the wordlist\n", warn);
	wordlistSize = 0;
	//counting how much \n exist in wordlist
	int len = strlen(wordlistRaw);
	for(int i = 0; i <= len; i++){
		if ( wordlistRaw[i] == '\n' || wordlistRaw[i] == '\0' ){
			wordlistSize++;
		}
	}

	char ** wordlist = malloc (sizeof (char * ) * wordlistSize);
	if ( !wordlist )
		return NULL;
	//todo: implementar o cut(\n)
	int szaux = 0;
	int cntaux = 0;
	char wordAux[32];
	for(int i = 0; i <= len; i++){
		//end of the word or blank line
		if ( wordlistRaw[i] == '\n' || wordlistRaw[i] == '\0' ){ 
			if ( szaux > 0 ){ //is not a blank line 
				//wordAux[szaux] = "";
				wordlist[cntaux] = malloc(szaux);
				if ( !wordlist[cntaux] ){
					error("Can't alloc memory space\n");
					return NULL;
				}
				strcpy( wordlist[cntaux], wordAux );
				szaux = 0;
				cntaux++;
			}
			continue;
		}
		//another char of the word
		wordAux[szaux++] = wordlistRaw[i];
	}

	wordlistSize = cntaux;
	if ( verbose )
		printf("%s wordlist cutted!\n", good);
	printf("%s Word list size: %d\n", good, wordlistSize);
	return wordlist;
}


char * prepareWordlistToSend(char ** wordlist){
	if ( verbose > 1 )
		printf("%s preparing wordlist to send...\n", warn);
	if ( verbose > 2 )
		printf("first element: %s\n", wordlist[currentIndexWordlist]);
	size_t size = nline * 10;
	int i;
	char * wordlistToSend = malloc(size);
	bzero(wordlistToSend, size );
	for( i = currentIndexWordlist; i < currentIndexWordlist + nline && i < wordlistSize; i++){
		strcat(wordlistToSend, wordlist[i]);
		strcat(wordlistToSend, "\n");
	}
	i -= currentIndexWordlist;
	currentIndexWordlist += i;
	if ( verbose > 2 )
		printf("last element: %s\n", wordlist[currentIndexWordlist - 1]);
	if ( verbose )
		printf("%s wordlist prepared to send! current index: %d\n", good, currentIndexWordlist);
	return wordlistToSend;
}

void * openFile(){
	char fileName[] = "wordlist.txt";
	FILE *file = fopen(fileName, "r");
	if (file == NULL){
		error("Error while opening the file.\n");
	}
	fseek(file, 0, SEEK_END); // seek to end of file
	long int size = ftell(file); // get current file pointer
	fseek(file, 0, SEEK_SET); // seek back to beginning of file
	if ( verbose )
		printf("%sFile size: %d\n", good, size);
	wordlistRaw = malloc(sizeof(char) * (size +1));
	strcpy(wordlistRaw, "");
	char ch;
	long int max = (size < MAX_CHAR_WORDLISTRAW? size: MAX_CHAR_WORDLISTRAW);
	char * save = wordlistRaw;
	for(int i = 0; i < max; i++){
		ch = fgetc(file);
		save[i] = ch;
	}
	fclose(file);
}