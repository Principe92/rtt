#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define LISTEN_BACKLOG 1
#define BUFFER_SIZE 2048
#define LIMIT 1024
#define NAMESIZE 32
#define OVERHEAD 16

#define handle_error(msg) \
   do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct param {
        char *mtype;
        int  probe;
        int  delay;
        int  size;
} CPhase;

void tryWrite(char *);
void exitChat();
void readPort();
int divide(char*, char[][LIMIT], const char *);
char getPhase(char*);
void errorOut(char*);
void parse(char *);
bool validCSP(char [][LIMIT], int);
bool validMP(char [][LIMIT], int);
bool validCTP(char [][LIMIT], int);
bool areEqual(char *, char *);
void closeCt();

int cfd = -1;
int phase = 0; // -1: Unknown, 0: CSP, 1: MP, 2: CTP
CPhase *csp;
int seq = 1; // probe sequence number starting at 1
char *data;
int len = 0;

// closes socket and exits
void closeCt(){
	shutdown(cfd, SHUT_RDWR);
	close(cfd);
	cfd = -1;
}

// closes the connection upon detecting that the client has disconnected
void exitChat(){
	printf("Client has disconnected\n");
	closeCt();
}

// writes an error message to the client and closes connection
void errorOut(char *msg){
	write(cfd, msg, strlen(msg)+1);
	printf("Closing client connection....\n");
	closeCt(cfd);
}

// reads in data from the socket
void readPort(){
	len = LIMIT;
	char buffer[LIMIT];
	data = malloc(len);
	bool av = false;

	while (cfd != -1){
		int size = recv(cfd, buffer, LIMIT, 0);

		if (size == 0) exitChat();

		else if (size < 0 && av) {
			parse(data); 
			av = false; 
		}

		else if (size > 0){ strncat(data, buffer, size); av = true; }			
		
		bzero(buffer, LIMIT);
	}
	
}

// check if two strings are equal
bool areEqual(char *a, char *b){

	if (strlen(a) != strlen(b)) return false;

	return !strncmp(a, b, strlen(b));
}

// check if the csp message is valid
bool validCSP(char msg[][LIMIT], int size){
	if (msg == NULL) return false;

	if (size != 5) return false;

	if (msg[0][0] != 's') return false;

	if (!areEqual(msg[1], "rtt") && !areEqual(msg[1], "tput")) return false;


	return true;
}

// check if mp message is valid
bool validMP(char msg[][LIMIT], int size){
	if (msg == NULL) return false;

	if (size != 3) return false;

	if (msg[0][0] != 'm') return false;

	if (strlen(msg[2])-1 != csp->size) return false;

	if (atoi(msg[1]) != seq) return false;

	if (atoi(msg[1]) > csp->probe) return false;

	return true;
}

// check if ctp message is valid
bool validCTP(char msg[][LIMIT], int size){
	if (msg == NULL) return false;

	if (size != 1) return false;

	if (msg[0][0] != 't') return false;

	return true;
}

// tries to write to the socket
void tryWrite(char *msg){
	int r = write(cfd, msg, strlen(msg)+1);
	if (r == -1) exitChat();
}

// parses the data
void parse(char *msg){
	char argv[LIMIT][LIMIT];
        const char delim = ' ';
        int size = divide(msg, argv, &delim);

	// Connection Setup Phase
	if (phase == 0){
		if (validCSP(argv, size)){
			phase += 1;
			tryWrite("200 OK: Ready\n");

			csp = malloc(sizeof(CPhase));
			csp->probe = atoi(argv[2]);
			csp->size = atoi(argv[3]);
			csp->delay = atoi(argv[4]);
			csp->mtype = argv[1];
			len = OVERHEAD + csp->size;
			data = malloc(len);

		}else errorOut("404 ERROR: Invalid Connection Setup Message\n");
	}


	// Measurement Phase
	else if (phase == 1){
		if (validMP(argv, size)){			
			sleep(csp->delay);
			tryWrite(msg);
			bzero(data, strlen(data));

			if (seq == csp->probe) phase += 1;
			else seq += 1;
			
		}else errorOut("404 ERROR: Invalid Measurement Message\n");
	}


	// Connection Termination Phase
	else if (phase == 2){
		if (validCTP(argv, size)){
			tryWrite("200 OK: Closing connection\n");
			closeCt();
			phase = 0;
			free(csp);
			seq = 1;
		} else errorOut("404 ERROR: Invalid Termination Message\n");
	}			
}

// divides a string into an array of substrings by a delimiter
int divide(char *source, char dest[][LIMIT], const char *delim){
        // Copy original string before tokenization
        /*char *original = malloc(strlen(source)+1);
        strcpy(original, source);
	int len = strlen(source);

	printf("Source: %s\n", source);
        char *chunk = strtok(source, delim);
        int count = 0;
	
	printf("First: %s\n", chunk);
        while (chunk != NULL){
		printf("%s\n", chunk);
                strcpy(dest[count++], chunk);
                chunk = strtok(NULL, delim);
        }

	source = original;*/

	int count = 0;

	int a = 0;
	int b = 0;
	int c = 0;
	int i = 0;
	int size = strlen(source);
	char *d = (char *)  delim;

	while(b < size){
		while (source[b] != d[0] && source[b] != '\n'){
		       dest[c][i++] = source[b++];
		}

		dest[c][b++] = '\0';
		i = 0;
		c++;
	}

        return c;
}
	

int main(int argc, char *argv[]){

	// check for arguments
	if (argc != 2){
		printf("Usage: ./server <Port number>\n");
		return 0;
	}

	int port = atoi(argv[1]);
	
	int sfd;
	int yes = 1;
	char text[BUFFER_SIZE];
	char *msg = malloc(BUFFER_SIZE);
	char *msg2 = malloc(BUFFER_SIZE);
        struct sockaddr_in my_addr, peer_addr;
        socklen_t peer_addr_size;

        sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sfd == -1) handle_error("Unable to open socket\n");

	// reuse socket
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes , sizeof(int)) < 0)
    		handle_error("Unable to reuse socket\n");

        my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));
	
	// Bind to port
        if (bind(sfd, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
             handle_error("Error occurred on bind");

	// Now listen for connection
        if (listen(sfd, LISTEN_BACKLOG) == -1)
             handle_error("Error occurred on listen");

	
        peer_addr_size = sizeof(struct sockaddr_in);

	while(1){
		while(cfd == -1)
        		cfd = accept4(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size, SOCK_NONBLOCK);

		fcntl(sfd, F_SETFL, O_NONBLOCK);

		if (cfd != -1) printf("Client has connected\n");
		
		readPort();
	}
}

