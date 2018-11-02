/*
to compille:
gcc -lcrypto -lpthread bruteforce.c -o bruteforce

usage: ./bruteforce host port

method code:
1 - SHA1

protocol definition:

client request:													server answer:

1 			- hash method										method
2 			- hash to decode									hash
3 			- wordlist id 										identifier to wordlist
4 			- wordlist size										wordlist size
5			- wordlist request									wordlist
6			- prossessing										nothing
7|id|hash	- not found in this id 								another wordlist id; if == -1 job is ended
8|id|index 	- pass found wordlist = id and position = index  	server don't answer


*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h> //to socket
#include <netinet/in.h> // to sockaddr_in 
#include <netdb.h> 	// to gethostbyname and others
#include <openssl/sha.h> //to SHA1 function, require compile with -lcrypto
#include <pthread.h> //threads

//includes for the warnings solve:
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct argsThread{
    int port;
    char *host;
};

struct argsWriteListenThread{
	int sock;
	int msgsz;
	char * msg;
};

void * dostuff (void * sock);
int listenWriteSock(int sock, int state);
int startConnection (struct argsThread * arguments);

char ** cutWordlist(char * wordlistRaw);
char * hashGenerator(int method, char * word);
int findhashin(char * hash, char ** wordlist); //-1 => not found | n => wordlist position found

//to print in verbose mode
char good[] = "\e[01;34m[\e[00m+\e[01;34m]\e[00m"; //[+] //Ighor's idea github.com/f0rb1dd3n/
char bad[] = "\e[01;31m[\e[00m-\e[01;31m]\e[00m";  //[-] //Ighor's idea github.com/f0rb1dd3n/
char warn[] = "\e[01;33m[\e[00m!\e[01;33m]\e[00m"; //[!] //Ighor's idea github.com/f0rb1dd3n/

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int verbose = 1;
int nextState;

//variables for protocol:
int method;
char * hashQuery = 0;
int wordlistId;
int wordlistSize = 0;
int wordlistRawSize = 0;
char * wordlistRaw = 0;
char ** wordlist = 0;
int position;


int main(int argc,  char *argv[]){
	int sockfd;

	if (argc < 3) { // necessita de 2 argumentos: host port
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    if (argc > 3)
    	verbose = atoi(argv[3]);

	struct argsThread arguments;
    arguments.port = atoi(argv[2]);
    arguments.host = argv[1];
    sockfd = startConnection(&arguments);

    pthread_t tid;
    int t = pthread_create(&tid, NULL, dostuff, (void *) &sockfd);
    pthread_join(tid, NULL); //waiting the thread finished

	/*
	char **wordlist = cutWordlist(wordlistRaw);
	int response = findhashin(hashQuery, wordlist);
	if ( response < 0 ){
		printf("NOT FOUND\n");
	}else{
		printf("Password found: %s\n", wordlist[response]);
	}

	
    if ( signalreceived ){
    	printf("%s hash received: %s\n", good, hashQuery);
    	printf("%s Wordlist received:\n%s\n",good, wordlistRaw);
    }
    */
    close (sockfd);
	return 0;
}


void * dostuff (void * sock){	
	int t;
	int sockno = *( (int *) sock );
	pthread_t tid, tid2;
	struct argsWriteListenThread args;
	nextState = 1;
	while ( nextState ){
		nextState = listenWriteSock(sockno, nextState);
		printf("Next state is: %d\n", nextState);
	}
	printf("method received: %d\n", method);
	printf("Hash received: %s\n", hashQuery);
	printf("wordlist id received: %d\n", wordlistId);
	printf("wordlist size received: %d\n", wordlistSize);
	//printf("wordlist received: %s\n", wordlistRaw);
	
	return NULL;
}

int listenWriteSock(int sock, int state){
	char * bufferW = 0;
	char * bufferL = 0;
	int n;
	int szW;
	int szL;

	if ( state == 1 ){	//hash method
		szW = 1;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "1");
		
		szL = 8;
	}
	if ( state == 2 ){ 	// hash
		szW = 1;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "2");

		szL = 1024;
	}
	if ( state == 3 ){ 	// wordlist id
		szW = 1;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "3");

		szL = 16;
	}
	if ( state == 4 ){ 	// wordlist size
		szW = 1;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "4");

		szL = 16;
	}
	if ( state == 5 ){ 	// wordlist 
		szW = 1;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "5");

		szL = wordlistRawSize;
	}
	if ( state == 6 ){ 	// processing
		//todo - make the findhashin be a monitored thread
		if ( verbose )
			printf("%s working...\n", good);
		position = findhashin (hashQuery, wordlist);//-1 => not found | n => wordlist position found
		if ( position == -1 ){ //not found
			return 7;
		}else{
			if ( verbose )
				printf("%s found password position: %d\npassword: %s\n", good, position, wordlist[position]);
			return 8;
		}
	}
	if ( state == 7 ){ 	// not found, request another wordlist id if = -1 the job is finished
		szW = 1050;
		bufferW = malloc(sizeof(char) * szW);
		bzero(bufferW, szW);
		strcpy(bufferW, "7");
		strcat ( bufferW, "|" );
		char aux[16];
		sprintf(aux, "%d", wordlistId);
		strcat ( bufferW, aux );
		strcat ( bufferW, "|" );
		strcat ( bufferW, hashQuery );

		szL = 16;
	}
	if ( state == 8 ){ 	//wordlist found, send the wordlist id and the position of the word
		szW = 128;
		bufferW = malloc ( sizeof(char) * szW );
		bzero(bufferW, szW);
		strcpy ( bufferW, "8" );
		strcat ( bufferW, "|" );
		char aux[16];
		sprintf ( aux, "%d", wordlistId );
		strcat ( bufferW, aux );
		strcat ( bufferW, "|" );
		char aux2[wordlistSize];
		sprintf( aux2, "%d", position );
		strcat ( bufferW, aux2 );

		szL = 16;
	}

	//writing
	if ( verbose > 1 )
		printf("%sSending a request: %d...\n", warn, state);
	//sleep(1);
	n = write(sock, bufferW, szW);

	if ( n < 0 ){
		printf("%s socket %d: ERROR writing on socket",bad, sock);
		if(bufferW){
			free (bufferW);
			bufferW = 0;
		}
		return 0;
	}
	if ( n == 0 ){
		printf("%s socket %d: Connection is close:",warn ,sock);
		if(bufferW){
			free (bufferW);
			bufferW = 0;
		}
		return 0;
	}
	if ( verbose )
		printf("%s request %d sent!\n", good, state);
	if(bufferW){
		free (bufferW);
		bufferW = 0;
	}
	bufferW = 0;
		
	//listening
	if ( verbose > 1 )
		printf("%slistening the answer request: %d...\n", warn, state);

	
	if ( state != 8 ){
		bufferL = malloc(sizeof(char) * szL);
		bzero(bufferL, szL);
		sleep(2);
		n = read(sock, bufferL, szL);
		if(verbose > 2)
			printf("allocating bufferL size: %d\n", szL);
		if ( n < 0 ){
			printf("%s socket %d: ERROR writing on socket",bad, sock);
			if(bufferL){
				free (bufferL);
				bufferL = 0;
			}
			return 0;
		}
		if ( n == 0 ){
			printf("%s socket %d: Connection is close:",warn ,sock);
			if(bufferL){
				free (bufferL);
				bufferL = 0;
			}
			return 0;
		}
		if ( verbose )
			printf("%s answer to request %d recievied!\n", good, state);
		if ( verbose > 3 )
			printf("received %c%c%c%c - %c%c%c%c\n", 
								bufferL[0], bufferL[1], bufferL[2], bufferL[3],
								bufferL[szL-5], bufferL[szL-4], bufferL[szL-3], bufferL[szL-2]);
	}

	if ( state == 1 ){ //hash method
		method = atoi ( bufferL );
		free ( bufferL );
		bufferL = 0;
		return state + 1;
	}
	if ( state == 2 ){ //hash
		hashQuery = bufferL;
		return state + 1;
	}
	if ( state == 3 ){ // wordlist id
		wordlistId = atoi ( bufferL );
		if( verbose > 1)
			printf("%s wordlist id received: %d\n",good, wordlistId);
		free ( bufferL );
		bufferL = 0;
		return state + 1;
	}
	if ( state == 4 ){ // wordlist size
		wordlistRawSize = atoi ( bufferL );
		free ( bufferL );
		bufferL = 0;
		if ( verbose > 1 )
			printf("%s wordlist size received: %d\n",good, wordlistRawSize);
		if ( wordlistRawSize )
			return state + 1;
		return 0;
	}
	if ( state == 5 ){ // wordlist
		wordlistRaw = bufferL;
		//cleaning memory space
		if ( wordlist ){ //problema encontrado! nao pode deletar wodlist, temque deletar cada elemento
			for(int i = 0; i < wordlistSize; i++){
				if(wordlist[i]){
					free(wordlist[i]);
					wordlist[i] = 0;
				}
			}
			free (wordlist);
			wordlist = 0;
		}
		wordlist = cutWordlist ( wordlistRaw );
		free (bufferL);
		bufferL = 0;
		return state + 1;
	}

	if ( state == 7 ){ // not found, request another wordlist size if = 0 the job is finished
		//printf("wordlistId recebido: %s\n", bufferL);
		wordlistId = atoi ( bufferL );
		free (bufferL);
		bufferL = 0;
		if( verbose )
			printf("%s wordlist id received: %d\n",good, wordlistId);
		if ( wordlistId >= 0 )
			return 4;
		return 0;
	}
	if ( state == 8 ) //wordlist found, send the wordlist id and the position of the word
		return 0;
}

int startConnection (struct argsThread * arguments){
    //struct argsThread * argument = (struct argsThread *) arguments;
    int sockfd, n;
    int port = arguments->port;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    if(verbose)
    	puts("Opening socket...");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    if(verbose)
    	printf("%s Socket opened!\n", good);
    if(verbose > 1)
    	printf("%sGetting host by name...\n", warn);
    server = gethostbyname((char *) arguments->host);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    if(verbose)
    	printf("%s Host found by name!\n", good);
    if(verbose > 1)
    	printf("%sConnecting...\n", warn);
    bzero((char *) &serv_addr, sizeof(serv_addr));  //zerando o buffer
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
       (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
		char str[80];
		strcpy(str, bad);
		strcat(str, " ERROR connecting");
        error(str);
    }
	if(verbose)
    	printf("%s Connected!\n", good);

    return sockfd;
}

char ** cutWordlist(char * wordlistRaw){
	wordlistSize = 0;
	long int max = strlen(wordlistRaw);
	/*
	//counting how much \n exist in wordlist
	for(int i = 0; i <= max; i++){
		if ( wordlistRaw[i] == '\n' || wordlistRaw[i] == '\0' ){
			wordlistSize++;
		}
	}
	*/
	int numLines = aDiff.text.Split('\n').Length;
	if(verbose > 2)
		printf("size wlr: %d  possible wordlistSize: %d\n", max, wordlistSize);
	
	char ** wordlist = malloc (sizeof (char * ) * wordlistSize);
	//good pratice
	for(int i = 0; i < wordlistSize; i++){
		wordlist[i] = 0;
	}
	if ( !wordlist )
		return NULL;
	puts("alocou");
	//todo: implementar o cut(\n)
	int szaux = 0;
	int cntaux = 0;
	char wordAux[32];
	bzero(wordAux, 32);
	for(int i = 0; i <= max; i++){
		//end of the word or blank line
		if ( wordlistRaw[i] == '\n' || wordlistRaw[i] == '\0' ){ 
			if ( szaux > 0 ){ //is not a blank line 
				wordAux[szaux] = '\0';
				wordlist[cntaux] = malloc(szaux);
				if ( !wordlist[cntaux] ){ //error
					error("Can't alloc memory space\n");
				}
				strcpy( wordlist[cntaux], wordAux );
				bzero(wordAux, 32);
				szaux = 0;
				cntaux++;
			}
			continue;
		}
		//another char of the word
		wordAux[szaux++] = wordlistRaw[i];
	}
	puts("passou do for"); //passa por aqui e para no free
	
	wordlistSize = cntaux;
	if(verbose > 2)
		printf("Wordlist cuted! Size: %d\nfirst element %s\nlast element %s\n",
											 wordlistSize, 
											 wordlist[0], 
											 wordlist[wordlistSize-1]);

	return wordlist;
}


char * hashGenerator(int method, char *word){
	if( method == 1 ){
		unsigned char hash[SHA_DIGEST_LENGTH];
		SHA1((unsigned char*) word, strlen(word), (unsigned char*) &hash);

		char *mdString = (char*) malloc(SHA_DIGEST_LENGTH*2+1);
	 
	    for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
	         sprintf(&mdString[i*2], "%02x", (unsigned int)hash[i]);
	 
	    return mdString;
	}

}

int findhashin(char * hash, char ** wordlist){
	char *mdString = 0;
	int signal;
	for (int i = 0; i < wordlistSize; i++){
		mdString = hashGenerator ( 1, wordlist[i] );
		signal = strcmp(hash, mdString);
		if ( mdString ){
			free( mdString );
			mdString = 0;
		}
		if ( signal == 0 ){ 
			return i;
		}
	}

	return -1;
}