/*******************************************************************************
 *
 * FILENAME : webproxy.c - a simple HTTP rate-limiting Proxy
 *
 * AUTHORS : Sebastian Carroll, Geoffrey Goldstraw
 *
 * CREATED : 5 May 12
 *
 * Version:          $Rev: 61 $
 * Date Modified:    $LastChangedDate: 2012-05-25 16:51:52 +1000 (Fri, 25 May 2012) $
 * Last Modified by: $Author: u4395897 $
 *
 * DESCR : Acts as a HTTP rate limiting proxy.
 *         Fulfils requirements 1-3 of Assign2 COMP3310, sem 1 2012
 *
 * TODO:
 *  8 May 12: Implement functionality to keep track of how many connections
 *            are running
 *
 ******************************************************************************/


#include <sys/types.h> 
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>  
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>


#include "relay_comms.h"
#include "defaults.h"
#include "config.h"

void fork_proc(int lis_sock,  struct config_sect *config_options, int rate_limiting);
void no_fork_proc(int lis_sock,  struct config_sect *config_options, int rate_limiting);


/* MAIN METHOD */
int main(int argc, char *argv[] )
{
  int sock_lis, debug_mode = 0, rate_limiting = 0;
  struct config_sect * config_options;

  // Check for config file and switch
  char *configFile = parseArgs(argc, argv, &rate_limiting);

  char * lis_port = malloc(10*sizeof(char));

  // Error occurred.
  if(lis_port == NULL){
      //handle this error properly
  }

  if(configFile != NULL){
    config_options = config_load(configFile);
    lis_port = extractListPort(config_options);
    debug_mode = extractDebugLevel(config_options);
  } else {
//     printf("No config file given\n");
    strcpy(lis_port, DEF_LIS_PORT);
    config_options = NULL;
  }
      
  // Start listening for incoming connections
  sock_lis = setup_socket(lis_port, NULL);
  
  free(lis_port);
  
  if(sock_lis < 0){
    printf("ERROR Could not set up listening socket");
    return -1;
  } 

 fork_proc(sock_lis, config_options, rate_limiting);
//   no_fork_proc(sock_lis, config_options, rate_limiting); //(Testing)

  close(sock_lis);
  return 1;
} /* main () */


// Relay loop without forking when there are waiting connections (DEBUGGING)
void no_fork_proc(int sock_lis,
		struct config_sect *config_options,
		int rate_limiting){
  int client_sock;

  if ( (client_sock = accept(sock_lis, NULL, NULL)) < 0){
      printf("ERROR in creating socket: %s", strerror(errno));
      return ;
  }

  relay(client_sock, config_options,rate_limiting );
  close(client_sock);
} // End no_fork_proc

// Fork of a new process for each waiting connection.
void fork_proc(int sock_lis,
		struct config_sect *config_options,
		int rate_limiting){

  int client_sock;
  pid_t fork_pid;

  // Enter infinite loop to respond to connections
  while(1){

      fprintf(stdout, "\nWaiting For connection\n");

      // wait till a connection can be accepted
      if ( (client_sock = accept(sock_lis, NULL, NULL)) < 0){
          printf("ERROR in creating socket: %s", strerror(errno));
          continue; // Go to the next loop and accept a new connection
      }

//       fprintf(stdout, "Connection Made: Forking child\n");
      fork_pid = fork();
      /* Code executed by child */
      if(fork_pid == 0){
//           fprintf(stdout, "IC: In child process\n");

          relay(client_sock, config_options, rate_limiting);

//           fprintf(stdout, "IC: Leaving child process\n");
      }
      else if(fork_pid < 0){
         printf("ERROR in creating fork");
      }
      close(client_sock);
  }
} // End fork_proc

