#include <stdio.h> 

#include <stdlib.h> 

#include <errno.h> 

#include <string.h> 

#include <netdb.h> 

#include <sys/types.h> 

#include <netinet/in.h> 

#include <sys/socket.h> 

#include <unistd.h>

#include <signal.h>

#include <omp.h>





#define MAXDATASIZE 100 /* max number of bytes we can get at once */



//functions declaration

	

void User_input(int socket_id);

void send_byte_size(int socketid, unsigned int bytes);

void Close_client();



char buffer [1024];

int sockfd, numbytes; 

int livefeed_off = 0;







int main(int argc, char *argv[])

{





signal(SIGINT, Close_client);

 

	char buf[MAXDATASIZE];

	struct hostent *he;

	struct sockaddr_in their_addr; /* connector's address information */

    int PORT;



	

	if (argc == 2){ //Defualt port value 12345 when no input

		PORT = 12345;

	}

	else if (argc == 3) {//input of port

		PORT = atoi(argv[2]);

	} else if ((argc > 3) || (argc == 1)){

		exit(EXIT_FAILURE);

	}





	if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */

		herror("gethostbyname");

		exit(1);

	}



	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

		perror("socket");

		exit(1);

	}



	their_addr.sin_family = AF_INET;      /* host byte order */

	their_addr.sin_port = htons(PORT);    /* short, network byte order */

	their_addr.sin_addr = *((struct in_addr *)he->h_addr);

	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */



	if (connect(sockfd, (struct sockaddr *)&their_addr, \

	sizeof(struct sockaddr)) == -1) {

		perror("connect");

		exit(1);

	}



	if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {

		perror("recv");

		exit(1);

	}



	buf[numbytes] = '\0';



	printf("Received: %s",buf);



	int livefeed_on = 0;





/*

implement multithreading with 2 threads using OpenMP

*/

#pragma omp parallel num_threads(2)

  { 

 #pragma omp sections

    {

        #pragma omp section /*threading for main*/

       { 



  while(1)

    {

		int n;

        bzero(buffer,1024);

        fgets(buffer,1023,stdin);

		

        n = send(sockfd,buffer,strlen(buffer), 0);

        if (n < 0) 

             perror("ERROR writing to socket");

		 



		if((buffer == NULL) || (strncmp("LIVEFEED" , buffer , 8) == 0)){

		livefeed_on = 0;

		

		}

		// if((strncmp("LIVEFEED" , buffer , 8) == 0)){

		// livefeed_off = 1;

		

		// }

		

        int i = strncmp("BYE" , buffer , 3);

        if(i == 0){

Close_client();

               break;

		}



    }

	   }



	           #pragma omp section /*threading for livefeed*/

       { 

		   while(1){

			   if(livefeed_on == 0){

          while(1){

			  		int n;

		   bzero(buffer,1023);

		 n = recv(sockfd,buffer,1024, 0);

        if (n < 0) 

         perror("ERROR reading from socket");

		 if(strncmp("....." , buffer , 6) == 0){

		 }else{

		    printf("%s\n",buffer);

		 }

		   

			}

			   }

		   }

	   }

	}

  }



printf("yesssss");

	close(sockfd);

	exit(0);



	return 0;

}



//close client 

void Close_client(){

	if(livefeed_off == 1){

		 send(sockfd, "STOP", 4, 0);

		 livefeed_off = 0;

	}else{

	printf("client is shutting down....");

	close(sockfd);

	exit(0);

	}

}