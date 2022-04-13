#include <omp.h>

#include <arpa/inet.h>

#include <stdio.h> 

#include <stdlib.h> 

#include <errno.h> 

#include <string.h> 

#include <sys/types.h> 

#include <netinet/in.h> 

#include <sys/socket.h> 

#include <sys/wait.h> 

#include <unistd.h>

#include <signal.h>

#include <sys/ipc.h>

#include <sys/shm.h>

#include    <pthread.h> 

#include <sys/mman.h>

#include <fcntl.h>

#include <semaphore.h>





	#define BACKLOG 10     /* pending connection it can handle */





/*

Shared int varibless

*/

int *message_counts; /* Count total messages */

int *client_id; /* Give each client unique ID */



/* This data structure is made of array of 1024

this means each message is saved in the data structure has 1024 bytes

also the client id will be saved corresponsing to each message

*/

 typedef struct message message_t;

 struct message

 {

    char message[1024];

    int clientid;

} ;

    



/*

channels save the message for each client

it was made as an array insted of linked list because we can share data

each message sent will have channelId

it can save total to 1000 messages

*/

 typedef struct channel channel_t;

  struct channel

  {

     int channelId [1000];

     message_t messages[1000];

 };

 channel_t *connection;





/*

Shared memory struct to count the number of messages for each channel

*/

  typedef struct counts_messages count_t;

  struct counts_messages

  {

     int ChannelId [256];

 };

 count_t *counts;





/*

This data structue is acting as linked list that holds data for each client which is local to each client

each client will not share this data

each channel a client subscribe the channel will be saved in channelid

*/

typedef struct channels_sub channels_sub_t;

struct channels_sub {

	int channel_id;

	int message_count;

	int read;

	int not_read;

	int channels_orders;

};





int channel_Number_next;

int channel_Number_livefeed;

//this node iterate throught the link list and allow modification of data in the link lisk

typedef struct node node_t;



// a node in a linked list of people

struct node {

    // channels_t *channels;

	channels_sub_t *channels_sub;

    node_t *next;

};





node_t *client = NULL; /* Declaration of node_t pointer to channels_sub_t */

pid_t childpid; /* Save Child Process */



char buffer [1024]; /* hold messages with max 1024 bytes */

int sockfd, new_fd; /*save sockfd and new_fd for each connection*/

int n;



/* Functions Headers */

void Close_server();

void channels_info(node_t *head, int newfd);

int check_channel(int channel_num, node_t *head);

void node_print(node_t *head);

void sub(char *command);

void unsub(int channel_num, int new_fd);

void send_message(char *command, int new_fd);

void next(char *command);

int message_count(int channel_id);

void node_client_print(node_t *head, int channels_id);

void client_print(channels_sub_t *p);

int count(channels_sub_t *p);

int count_unread(node_t *head, int channels_id);

int unread_message(int channel_id);

void next_message(int channels_id, int message_postion);

void send_toclient(int i);

void livefeed(char *command);

void messages_count(channels_sub_t *p);

void node_count(node_t *head);

void update_status();

void send_all_toclient(int i);

void next_all_messages(int message_postion);

int count_all(channels_sub_t *p);

int count_all_unread(node_t *head);

int unread_all_message();

void lvfed_no_chnlid();

void next_no_chnlnum();

int count_next_unread(node_t *head);

node_t * node_add1(node_t *head, channels_sub_t *channels_sub_t);

node_t * node_delete(node_t *head, int chan_id);



 

pthread_mutex_t* process_mutex;  /* This lock for process sync and shared data */

pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER; /*mutex lock declaration for threads*/



/*switches used to turn on/off next/livefeed threads*/

int value; 

int livefeed_switch = 0;

int next_switch = 0;





int messages_queue = 2; /*Keep track of messages for Next funciton*/



int main(int argc, char *argv[])

{



	/* Declaration of share memory using mmap */

    message_counts = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 

                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

					*message_counts = 0;

    connection = mmap(NULL, sizeof(channel_t), PROT_READ | PROT_WRITE, 

                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    counts = mmap(NULL, sizeof(count_t), PROT_READ | PROT_WRITE, 

                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    client_id = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, 

                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

					*client_id = 0;

        /* Declare mutex lock in the shared memory using mmap to handle process sync*/

    process_mutex=(pthread_mutex_t*)mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);  

   if( MAP_FAILED==process_mutex )  

    {  

       perror("mmap");  

       exit(1);  

    }  



         /*Initialisation of process mutex lock*/

   pthread_mutexattr_t attr;  

   pthread_mutexattr_init(&attr); 



      value=pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);  

   if( value!=0 )  

   {  

       perror("init_mutex pthread_mutexattr_setpshared");  

       exit(1);  

   }  

   pthread_mutex_init(process_mutex, &attr);   



    signal(SIGINT, Close_server); /* Signal to exit the server gracfully */



	int MYPORT; /* port number */

	struct sockaddr_in my_addr;    /* address information */

	struct sockaddr_in their_addr; /* connector's address information */

	socklen_t sin_size;



	if (argc == 1){ /*Defualt port value 12345 when no input*/

		MYPORT = 12343;

	}

	else if (argc == 2) { /* Save input in MYPORT */

		MYPORT = atoi(argv[1]);

	}else {

		exit(EXIT_FAILURE);

	}



	/*Generate the socket*/

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

		perror("socket");

		exit(1);

	}



	/* generate the end point */

	my_addr.sin_family = AF_INET;         /* host byte order */

	my_addr.sin_port = htons(MYPORT);     /* short, network byte order */

	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

		/* bzero(&(my_addr.sin_zero), 8);   ZJL*/     /* zero the rest of the struct */



	/*bind socket to the port*/

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) \

	== -1) {

		perror("bind");

		exit(1);

	}



	/* start listnening */

	if (listen(sockfd, BACKLOG) == -1) {

		perror("listen");

		exit(1);

	}



	printf("server starts listnening ...\n");



	while(1) {  /* main accept() loop */

		sin_size = sizeof(struct sockaddr_in);

			/* Wait for conneciton and accept them */

		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, \

		&sin_size)) == -1) {

			perror("accept");

			continue;

		}

		printf("server: got connection from %s\n", \

			inet_ntoa(their_addr.sin_addr));



      /* Make child process for each connection when fork equal zero */

		if ((childpid = fork()) == 0) {

			 /* this is the child process */



		/*send a welcoming message to client with client uniqe ID*/

		char welcome_message [50];

		snprintf(welcome_message, sizeof(welcome_message), "Welcome! Your client ID is %d \n", *client_id);

			if (send(new_fd, welcome_message, sizeof(welcome_message), 0) == -1)

				perror("send");



		++*client_id; /* Increase to give new value to new client */

         char *command; /* Save user input and validate it */



/*

Creating thredts using OpemMP

3 predefinde threads was given:



1. for main

2. for livefeed 

3. for next



Each thread inside separated section

*/

#pragma omp parallel num_threads(3) /*declaration for number of threads*/

  { 

 #pragma omp sections

    {

        #pragma omp section /* thread section handling main */

       { 

	while(1){ /* loop to run main */



           bzero(buffer,1024); /* buffer set to zeros */

           n = recv(new_fd,buffer,1024, 0); /*recive input from client and saved in buffer*/

           if (n < 0){

		   perror("ERROR reading from socket"); /*close if error*/

		   close(sockfd);

		   close(new_fd);

		   exit(0);

		   }





         command = NULL; /* set command to Null  */

		 command = strtok(buffer, " "); /*split the string and take the first value*/



         update_status(); /*update message count, read, unread */



		if(strncmp("SUB" , command, 3) == 0){ /* SUB to channels */

	     sub(command); /* Pass command to sub functon */

		 }

		else if(strncmp("CHANNELS" , command, 8) == 0){ /* Validate CHANNELS command */

     	 channels_info(client, new_fd); /*pass client and new_fd to functon*/

		 }

		else if(strncmp("UNSUB" , command, 5) == 0){ /* Validate UNSUB command */

	     	/* split the string to obtain channel number only and pass to unsub function */

		 command = strtok(NULL, " "); 

		 int channel_num = 0;

		 channel_num += atoi(command);

		// printf("%d\n", channel_num);

		 unsub(channel_num, new_fd);

		 }



		 /* This will activate SEND */

		else if(strncmp("SEND" , command, 4) == 0){/* Validate SEND command */

		 send_message(command, new_fd);

		 }		 



		else if(strncmp("NEXT" , command, 4) == 0){ /* Validate NEXT command */

    

		 if( NULL == (command = strtok(NULL, " "))){	/* check if channel number provided */			 

			next_switch = 1; /* this means only run next without channel id*/

	 	 }else{

		 char *next_response = malloc(sizeof(char)*100);

		 int channel_num = 0;

		 	 channel_num += atoi(command);

    		// char *next_response = malloc(sizeof(char)*100);

				//validate the channel number btween 0-255

		 	 if(channel_num >= 0 && channel_num <= 255){

				//if check channel return 1 means channel is not exist

				if(check_channel(channel_num, client) == 1){

			 	 sprintf(next_response, "Not subscribed to channel %d", channel_num);		 

    			 n = send(new_fd,next_response, strlen(next_response) , 0);

    				 if (n < 0) perror("ERROR w riting to socket");

	        	}else{	

				 channel_Number_next = 0;

	             channel_Number_next += atoi(command);

				 n = send(new_fd,"next activated\n",15, 0);

				 next_switch = 2;



					}

 				 }

 				 else{//we need to implement next without any thing NEXT\0

 			 	 sprintf(next_response, "Invalid channel: %d.", channel_num);		 

 			     n = send(new_fd,next_response, strlen(next_response) , 0);

 			     if (n < 0) perror("ERROR w riting to socket");

 				 }

	           free(next_response);

	           next_response = NULL;	



			}

	   

	     }



		else if(strncmp("LIVEFEED" , command, 8) == 0){ /* Validate Livefeed command */



		 char *next_response = malloc(sizeof(char)*100);

			int channels_sum = count_next_unread(client);

		 if( NULL == (command = strtok(NULL, " "))){				 

	   	      if(channels_sum == 0){

				 strcpy(next_response, "Not subscribed to any channels");

				 n = send(new_fd, next_response, strlen(next_response), 0);

			 }else{



				 n = send(new_fd,"Livefeed activated\n",25, 0);

			     livefeed_switch = 1;



			 }



		 }else{



		 int channel_num = 0;

		 	 channel_num += atoi(command);

			 channel_Number_livefeed = 0;

	         channel_Number_livefeed += atoi(command);

    		// char *next_response = malloc(sizeof(char)*100);

				//validate the channel number btween 0-255

		 	 if(channel_num >= 0 && channel_num <= 255){

				//if check channel return 1 means channel is not exist

				if(check_channel(channel_num, client) == 1){

			 	 sprintf(next_response, "Not subscribed to channel %d", channel_num);		 

    			 n = send(new_fd,next_response, strlen(next_response) , 0);

    				 if (n < 0) perror("ERROR w riting to socket");

	        	}else{	

					

	              channels_sum = count_next_unread(client);

				 if(channels_sum == 0){



					 strcpy(next_response, "Not subscribed to any channels");

					 n = send(new_fd, next_response, strlen(next_response), 0);



				 }else{

				 livefeed_switch = 2;

				 n = send(new_fd,"Livefeed activated\n",25, 0);

				 }

					}

 				 }

 				 else{//we need to implement next without any thing NEXT\0

 			 	 sprintf(next_response, "Invalid channel: %d.", channel_num);		 

 			     n = send(new_fd,next_response, strlen(next_response) , 0);

 			     if (n < 0) perror("ERROR w riting to socket");

 				 }





			}

	           free(next_response);

	           next_response = NULL;	

		 



		 }

		else if(0 == strncmp("BYE" , buffer, 3)){/* Validate BYE command */

			   	close(sockfd);

				close(new_fd);

	            exit(0);

	            break;

		   }

		else if(strncmp("STOP" , command, 4) == 0){/* Validate STOP command */

			   n = send(new_fd,"Deactivat Livefeed/Next\n",26, 0);

				livefeed_switch = 0;

				next_switch = 0;

			   }

	    else { /* when incorrect value is entered*/  



		    }



		   if(n == 0){

	          

			   	close(sockfd);

			   	close(new_fd);

	            exit(0);

	            break;

		   }



		}

	   }



     #pragma omp section /*Second thread for Livefeed in background*/

       {

		 while(1){

		 	 if(livefeed_switch == 1){

				lvfed_no_chnlid();

			  }else if(livefeed_switch == 2){

			      livefeed(command);

		  }

		 }

	    }

     #pragma omp section /*third thread for Livefeed in background*/

       {

		 while(1){

		 	 if(next_switch == 1){

				next_no_chnlnum();

				next_switch = 0;

			  }else if(next_switch == 2){

			      next(command);

				next_switch = 0;

		  }

		 }

	    }



	}

  }

		while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up child processes */

	}



		close(new_fd);  /* parent doesn't need this */



}

}





void next_all_message(int channels_id, int message_postion){

	// printf("message postion is : %d\n", message_postion);

	 

	int test = message_postion;





 

    for (int i=(*message_counts - 1); i>=0; --i) {



		if((connection->channelId[i] == channels_id)){

		--test;

		if(test == 0){

        send_toclient(i);

		}

    }

  }

       

}



/*

count the unread messages

*/

int count_next_unread(node_t *head){

     int unread_count = 0;

    for ( ; head != NULL; head = head->next) {

		 pthread_mutex_lock(&threads_mutex);

		++unread_count;

		 pthread_mutex_unlock(&threads_mutex);

    }

	return unread_count;

}



/*

function that tracks the position of messages

*/

void next_megs(int channels_id, int message_postion){

	// printf("message postion is : %d\n", message_postion);

	 

	int test = message_postion;





 

    for (int i=(*message_counts - 1); i>=0; --i) {



		if((connection->channelId[i] == channels_id)){

		--test;

		if(test == 0){

        send_all_toclient(i);

		}

    }

  }



}





/*

deals with channel number

*/

void next_no_chnlnum(){

    char *next_response = malloc(sizeof(char)*100);

	int channels_sum = count_next_unread(client);

	int channels_order [channels_sum];

	int channelsid [channels_sum];

	int channel_num = 1;



    node_t *head = client;

	int i = 0;

    for ( ; head != NULL; head = head->next) {

		



	 channels_order [i] = head->channels_sub->channels_orders;

	 channelsid [i] = head->channels_sub->channel_id;

	 ++i;

    }

	



	int a;

     for (i = 0; i < channels_sum; ++i) 

    {

        for (int j = i + 1; j < channels_sum; ++j) 

        {

            if (channels_order[i] < channels_order[j]) 

            {

                a = channels_order[i];

                channels_order[i] = channels_order[j];

                channels_order[j] = a;



                a = channelsid[i];

                channelsid[i] = channelsid[j];

                channelsid[j] = a;



            }

        }

    }







 	if(channels_sum == 0){

 		 strcpy(next_response, "Not subscribed to any channels");

 		 n = send(new_fd, next_response, strlen(next_response), 0);

 	}

 	else{

 	for(i=0; i<channels_sum; ++i){

	 



    

		int message_exist = 0;

		update_status();

		 message_exist += unread_message(channelsid[i]); 



		if(message_exist == 0){

		 strcpy(next_response, " ");

		 n = send(new_fd,next_response, strlen(next_response) , 0);

		 if (n < 0) perror("ERROR w riting to socket");	

		}else{

		next_megs(channelsid[i], message_exist);

	 

		}

		

  }

 }

	 free(next_response);

	 next_response = NULL;	 

}





/*

function that handles sending messages to client

*/

void send_toclient(int i) {



		 n = send(new_fd,connection->messages[i].message

		 , strlen(connection->messages[i].message) , 0);

	 if (n < 0) perror("ERROR w riting to socket");

}



/*

function that handles next command to display next messages

*/

void next_message(int channels_id, int message_postion){

	// printf("message postion is : %d\n", message_postion);

	 

	int test = message_postion;

 

    for (int i=(*message_counts - 1); i>=0; --i) {

	 pthread_mutex_lock(&threads_mutex);

		if((connection->channelId[i] == channels_id)){

		--test;

		if(test == 0){

        send_toclient(i);

		}

    }

	 pthread_mutex_unlock(&threads_mutex);

  } 



}



/*

function to send all messages to client

*/

void send_all_toclient(int i) {



	char meg_channelid[1024];



	int ret = snprintf(meg_channelid, sizeof(meg_channelid), "%d:%s"

	, connection->channelId[i], connection->messages[i].message);

	   if (ret < 0) {

      //   abort();

    }

	

		 n = send(new_fd,meg_channelid

		 , strlen(meg_channelid) , 0);

	 if (n < 0) perror("ERROR w riting to socket");



	//  free(meg_channelid);

	//  meg_channelid = NULL;

}



/*

display all the next messages

*/

void next_all_messages(int message_postion){

	// printf("message postion is : %d\n", message_postion);

	 

	int test = message_postion;





 

    for (int i=(*message_counts - 1); i>=0; --i) {







		--test;

		if(test == 0){

		 send_all_toclient(i);



		}

    }

  }





/*

counts all messages 

*/

int count_all(channels_sub_t *p) {



	if(p->not_read > 0){

	 pthread_mutex_lock(&threads_mutex);

	--p->not_read;

	++p->read;

	 pthread_mutex_unlock(&threads_mutex);

	return (p->not_read + 1);

	}else if(p->not_read == 0){

		return 0;

	}



  }





/*

counts all unread messages

*/

int count_all_unread(node_t *head){

     int unread_count = 0;

    for ( ; head != NULL; head = head->next) {





       unread_count += count_all(head->channels_sub);



    }

	return unread_count;

   }



	int unread_all_message(){

	int messages_unread = count_all_unread(client);

	return messages_unread;

}





/*

handling livefeeds with messages count

*/

void lvfed_no_chnlid(){

	

		while(livefeed_switch == 1){

		int message_exist = 0;

			update_status();

			

 



		 message_exist += unread_all_message();



		if(message_exist == 0){

		}else{





  // printf("THIS IS MESSAGES COUNT: %d\n", message_exist);

			next_all_messages(message_exist);

	 

			}

	}



}





/*

implement LIVEFEED command functionality

*/

void livefeed(char *command){

	  int channel_num = channel_Number_livefeed;



			while(livefeed_switch == 2){

			int message_exist = 0;

				update_status();

				

			 message_exist += unread_message(channel_num);



			if(message_exist == 0){

			}else{



			next_message(channel_num, message_exist);

	 

			}

	}



}





/*

return not read messages count

*/

int count(channels_sub_t *p) {

	if(p->not_read > 0){

	pthread_mutex_lock(&threads_mutex);

	--p->not_read;

	++p->read;

	pthread_mutex_unlock(&threads_mutex);

	return (p->not_read + 1);

	}else if(p->not_read == 0){

		return 0;

	}



 }







/*

return unread count

*/

int count_unread(node_t *head, int channels_id){

     int unread_count = 0;

    for ( ; head != NULL; head = head->next) {



		if(head->channels_sub->channel_id == channels_id){

       unread_count = count(head->channels_sub);

		}

    }

	return unread_count;

}





/*

return number of unread messages in specific channel

*/

int unread_message(int channel_id){

	int messages_unread = count_unread(client, channel_id);

	return messages_unread;

}





/*

implement NEXT command functionality

*/

void next(char *command){

    //	 command = strtok(NULL, " ");

	 int channel_num = channel_Number_next;

	// channel_num += atoi(command);

	 //Allocate memory for response

     char *next_response = malloc(sizeof(char)*100);

	// printf("this is channels number:%d", channel_num);

 			//do this now brave

			update_status();



			int message_exist = 0;

			 message_exist += unread_message(channel_num); //  Fix this shit  //



			if(message_exist == 0){

				//printf("there is nothing");

			 strcpy(next_response, " ");

			 n = send(new_fd,next_response, strlen(next_response) , 0);

    		 if (n < 0) perror("ERROR w riting to socket");	

			}else{

			next_megs(channel_num, message_exist);

			sprintf(next_response, "next running brav: %d", channel_num);		 

			}

	 free(next_response);

	 next_response = NULL;	 

}



/*

implement SUB command functionality

*/

void sub(char *command){

	 command = strtok(NULL, " ");

	 int channel_num = 0;

	 channel_num += atoi(command);

	// printf("%d\n", channel_num);

	 

	 //Allocate memory for response

    char *sub_response = malloc(sizeof(char)*100);

	//validate the channel number btween 0-255

	 if(channel_num >= 0 && channel_num <= 255){

		 //make sure if client already subscribed and send back a message

		if(check_channel(channel_num, client) == 1){

		  sprintf(sub_response, "Subscribed to channel %d.", channel_num);		 

	      n = send(new_fd,sub_response, strlen(sub_response) , 0);

          if (n < 0) perror("ERROR w riting to socket");



                channels_sub_t *beth = (channels_sub_t *)malloc(sizeof(channels_sub_t)); // allocate memory for beth on the free store

			  if (beth == NULL) {

 					  printf("memory allocation failure\n");

				  }

         

            //find the meassge unread and message read and other stuff and assign to zero

			    beth->channel_id = channel_num;

			    beth->read = 0;

			    beth->not_read = 0;

				beth->message_count = message_count(channel_num);

				beth->channels_orders = 1;

			// printf("\n");

		    node_t *newhead = node_add1(client, beth);

 		   if (newhead == NULL) {

 		       printf("memory allocation failure\n");

  			  }

			 pthread_mutex_lock(&threads_mutex);

		    client = newhead;

			 pthread_mutex_unlock(&threads_mutex);

		}

		else{

		  sprintf(sub_response, "Already subscribed to channel %d.", channel_num);		 

	      n = send(new_fd,sub_response, strlen(sub_response) , 0);

          if (n < 0) perror("ERROR w riting to socket");

		}

	 }

	 else{

		sprintf(sub_response, "Invalid channel: %d.", channel_num);		 

	    n = send(new_fd,sub_response, strlen(sub_response) , 0);

       if (n < 0) perror("ERROR w riting to socket");

	 }

	 free(sub_response);

	 sub_response = NULL;

}



/*

implement channels in the queue

*/

void channel_queue(channels_sub_t *p) {

	 pthread_mutex_unlock(&threads_mutex);

		p->channels_orders = messages_queue;

	 pthread_mutex_unlock(&threads_mutex);

}



/*

updates the queue of channels

*/

void update_queue(node_t *head, int channels_number) {	

    for ( ; head != NULL; head = head->next) {

		

		if(head->channels_sub->channel_id == channels_number){	

        channel_queue(head->channels_sub);

		}

    }

}





/*

implement sending a message

*/

void send_message(char *command, int new_fd){



     char *response = malloc(sizeof(char)*100);

	 command = strtok(NULL, " ");

	  int channel_num = 0;

	  channel_num += atoi(command);

	 command = strtok(NULL, "");



	 if((channel_num >= 0 && channel_num <= 255) && command != NULL){



	 char *meg = malloc(sizeof(char)*1024);

  	strcat(meg, command);



		/*This is a critical section and here shared memory is modified - Needs lock*/

			       value=pthread_mutex_lock(process_mutex);  

   				    if( value!=0 )  perror("Error Locking"); 



  				connection->channelId[*message_counts] = channel_num;

				strcpy(connection->messages[*message_counts].message, meg);

				//increase messages count whenever sending a message

				++*message_counts;

			    ++counts->ChannelId[channel_num];



		       value=pthread_mutex_unlock(process_mutex);    

                if( value!=0 ) perror("Error Unlocking"); 





			update_queue(client, channel_num);



			/* Lock the messages_queue modification*/

		     value=pthread_mutex_lock(process_mutex);  

            if( value!=0 )  perror("Error Locking"); 



			++messages_queue;



 	       value=pthread_mutex_unlock(process_mutex);    

           if( value!=0 ) perror("Error Unlocking"); 

	 }

	 else{

 	  sprintf(response, "Invalid channel: %d.", channel_num);

		 n = send(new_fd,response, strlen(response) , 0);

         if (n < 0) perror("ERROR w riting to socket");

	 }

	 free(response);

	 response = NULL;

}





/*

close the server

*/

void Close_server(){

	 n = send(new_fd,"SUHTDOWN", 8 , 0);

    if (n < 0) perror("ERROR w riting to socket");

	while(waitpid(-1,NULL,WNOHANG) > 0); 

	printf("Server is shutting down....");

   	close(sockfd);

	close(new_fd);

	munmap(connection, sizeof(channel_t));

	munmap(message_counts, sizeof(int));

	munmap(counts, sizeof(count_t));

    munmap(process_mutex, sizeof(pthread_mutex_t));

	munmap(client_id, sizeof(int));

	exit(0);

}



/*Make function channels to display all channels information*/

void channels_info(node_t *head, int newfd){

	//the nummber of channesl

	int max_num_channels = 255;

		//allocate memory for channels information

	    char *channesls_info = malloc(sizeof(char)*90*max_num_channels);



	  for ( ; head != NULL; head = head->next) {

	    char *add_string = malloc(sizeof(char)*90);



	sprintf(add_string, "Channel ID:%d\tMessage Count:%d\tRead Messages:%d\tUnread Measseges:%d\n"

	,head->channels_sub-> channel_id, head->channels_sub->message_count

	 ,head->channels_sub-> read, head->channels_sub->not_read);



	//add both the strings in one

  	strcat(channesls_info, add_string);



	free(add_string);

	add_string = NULL;

    }



	//printf("chananele ino results: %s\n", channesls_info);

	 n = send(newfd,channesls_info ,strlen(channesls_info)+1, 0);

	  if (n < 0) perror("ERROR w riting to socket");

}



/*make sure channel is not already subscribed*/

int check_channel(int channel_num, node_t *head){

	    for ( ; head != NULL; head = head->next) {

		if(channel_num == head->channels_sub->channel_id){

			return 0;

		}

    }

	return 1;

}



/*count all messages related to thhe channel id in the link list*/

int message_count(int channel_id){

	int mes_count = 0;

	 for (int i=0; i<*message_counts; ++i) {

		 if(channel_id == connection->channelId[i]){

			 ++mes_count;

		 }

    }

	return mes_count;

}



/* Add new node to a linked list*/

node_t * node_add1(node_t *head, channels_sub_t *channels_sub) {

    // create new node to add to list

    node_t *new = (node_t *)malloc(sizeof(node_t));

    if (new == NULL) {

        return NULL;

    }



    // insert new node

	 pthread_mutex_lock(&threads_mutex);

    new->channels_sub = channels_sub;

    new->next = head;

	 pthread_mutex_unlock(&threads_mutex);

    return new;

}





void messages_count(channels_sub_t *p) {



	/*Lock access to local data*/

		pthread_mutex_lock(&threads_mutex);



		p->not_read += (counts->ChannelId[p->channel_id] - p->message_count);

		p->message_count = counts->ChannelId[p->channel_id];

	/*Unlock the access*/

		 pthread_mutex_unlock(&threads_mutex);



}





/*

counts number of nodes

*/

void node_count(node_t *head) {	

    for ( ; head != NULL; head = head->next) {

        messages_count(head->channels_sub);

    }

}





/*

update current status for client

*/

void update_status(){



  /* Critical section to be locked down */ 

	node_count(client);

 

}



/*

implement UNSUB command functionality

*/

void unsub(int channel_num, int new_fd){



 	//Allocate memory for response

	char *unsub_message = malloc(sizeof(char)*200);

	//validate the channel number btween 0-255

 	if(channel_num >= 0 && channel_num <= 255) {

	 //make sure if client already subscribed and send back a message

	if(check_channel(channel_num, client) == 1){

	  sprintf(unsub_message, "Not subscribed to channel %d.", channel_num);		 

      n = send(new_fd,unsub_message, strlen(unsub_message) , 0);

      if (n < 0) perror("ERROR w riting to socket");

  	 }

	  else{

		  client = node_delete(client, channel_num);

		  sprintf(unsub_message, "Unsubscribed from channel %d.", channel_num);	

          n = send(new_fd,unsub_message, strlen(unsub_message) , 0);

         if (n < 0) perror("ERROR w riting to socket");		  

	  } 

  	}else{

	  sprintf(unsub_message, "Invalid channel: %d.", channel_num);		 

      n = send(new_fd,unsub_message, strlen(unsub_message) , 0);

      if (n < 0) perror("ERROR w riting to socket");	  		  

	  }

	  free(unsub_message);

	  unsub_message = NULL;

}



/*this deletes elements from delete*/

node_t * node_delete(node_t *head, int chan_id) {

    node_t *previous = NULL;

    node_t *current = head;

	// pthread_mutex_lock(&threads_mutex);

    while (current != NULL) {

        if (current->channels_sub->channel_id == chan_id) {

            node_t *newhead = head;

            if (previous == NULL)  // first item in list

                newhead = current->next;

            else 

                previous->next = current->next;

            free(current);

            return newhead;

        }

        previous = current;

        current = current->next;

    }

	// pthread_mutex_unlock(&threads_mutex);

    // name not found

    return head;

}