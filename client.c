#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>          
#include <unistd.h>          
#include <sys/types.h>
#include <sys/socket.h>         
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>          
#include <malloc.h>
#include <stdbool.h>
#include <sys/time.h>

#define LIMIT 256

int    socketfd;
struct sockaddr_in servaddr;
char   buffer[LIMIT];
char   csp[(32000/sizeof(char))];
int    next = 0;
char   *buf;

void* chat_write(int);
void* chat_read(int);
int divide(char*, char[][LIMIT], const char*);
bool areEqual(char *, char *);
void wait();
void calc(char*, int, int, float[]);

typedef struct param {
        char phase;
        char *mtype;
        int  probe;
	int  delay;
	int  sfd;
	int *mSizes;
} Param;


int main(int argc, char *argv[]){

	pthread_t readth,writeth;

	// check for invalid number of arguments
     	if(argc != 3){
      	    printf("Usage: ./client <IP address> <Port Number>\n");
       	    exit(0);
       	}

      	// create socket
      	socketfd = socket(AF_INET, SOCK_STREAM, 0);
      	if (socketfd == -1){
            printf ("Unable to create socket\n");
	    exit(0);
	}

      	// set ip and port
      	bzero((char *) &servaddr, sizeof(servaddr));
      	servaddr.sin_family = AF_INET;
      	servaddr.sin_port = htons(atoi(argv[2]));
      	servaddr.sin_addr.s_addr = inet_addr(argv[1]);

      	// try to connect
      	if(connect(socketfd, (struct sockaddr *) &servaddr, sizeof(servaddr))==-1)
      	{
            printf("Unable to connect client to server \n");
            exit(0);
      	}
      	else
     	   printf("Connected to server\n");
    


	// create read and write threads
      	pthread_create(&writeth, NULL,  (void *)chat_write, (void *)(intptr_t)socketfd);
      	pthread_create(&readth,  NULL,  (void *)chat_read,  (void *)(intptr_t)socketfd);    
      	pthread_join(writeth, NULL);
      	pthread_join(readth,  NULL);

      	return 0;
 }

 void* chat_read(int socketfd)
 {
	bool av = false;

        while(1){
	    	int n = recv(socketfd, buffer, LIMIT-1, MSG_DONTWAIT);
        	
		// server has closed 	
  	    	if(n==0) exit(0);

		// we have read some data
	    	else if(n>0){
        		//printf("%s", buffer);

            		bzero(buffer, strlen(buffer));
			av = true;
             	}

		// no data available to read
		else if (n<0 && av) {
			__sync_add_and_fetch(&next, 1);
			av = false;
		}
     	 }
 }

 
 void* chat_write(int socketfd){

	while(1){
		// Connection SetUp Phase
		printf("\n~$: ");
		fgets(buffer, LIMIT-1, stdin);
		
		sprintf(csp, "%s\n", buffer);
		write(socketfd, buffer, strlen(csp)+1);

		// Message Phase
		wait();
		
		// parse input
		char argv[LIMIT][LIMIT];
	        const char delim = ' ';
        	int size = divide(csp, argv, &delim);

		int probe = atoi(argv[2]);
		int msgSize = atoi(argv[3]);
		int seq = 1;

		char msg[msgSize];
		float mean[probe];
		memset(msg, 'a', msgSize);
		msg[msgSize] = '\0';

		for(seq; seq <= probe; seq++){
			sprintf(csp, "m %d %s\n", seq, msg);
			struct timespec end, start;
			clock_gettime(CLOCK_MONOTONIC_RAW, &start);

			// send probe message
			write(socketfd, csp, strlen(csp)+1);

			wait();

			clock_gettime(CLOCK_MONOTONIC_RAW, &end);

			// calculate rtt time
			mean[seq-1] = (end.tv_sec - start.tv_sec) + ((end.tv_nsec - start.tv_nsec)/1000000000.0f);
			bzero(csp, LIMIT);
		}
		
		calc(argv[1], probe, msgSize, mean);

		// Connection Termination Phase
		sprintf(csp, "t\n");
		write(socketfd, csp, strlen(csp)+1);

		wait();
	}
 }

void calc(char *msg, int probes, int size, float mean[]){
	float sum = 0;
        int i = 0;
        for(i; i < probes; i++) sum += mean[i];

	float f = sum/(probes/1.0f);

	if (areEqual(msg, "rtt")){
		printf("$: Roundtrip time is: %.4f s\n", f);
	}

	else if (areEqual(msg, "tput")){
                printf("$: Throughput is: %.4f bps\n", (size/1.0f)/f);
	}
}

// divides a string into an array of substrings by a delimiter
int divide(char *source, char dest[][LIMIT], const char *delim){
        // Copy original string before tokenization
        char *original = malloc(strlen(source)+1);
        strcpy(original, source);


        char *chunk = strtok(source, delim);
        int count = 0;

        while (chunk != NULL){
                strcpy(dest[count++], chunk);
                chunk = strtok(NULL, delim);
        }

        strcpy(source, original);

        return count;
}

// check if two strings are equal
bool areEqual(char *a, char *b){

        if (strlen(a) != strlen(b)) return false;

        return !strncmp(a, b, strlen(b));
}

void wait(){
	while(next == 0) continue;
	__sync_sub_and_fetch(&next, 1);
}


