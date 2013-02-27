/******************************** relay_comms.c **************************
 Description:
  Contains functions to rate limit and relay HTTP messages between client and
  server.

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 58 $
 Date Modified:    $LastChangedDate: 2012-05-25 16:35:41 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4395897 $

 BUGS:
 *  5 may 12: Fork code leaves defunct processes around.
             Not sure how to clean these up

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>

#include <netdb.h>

#include <sys/socket.h>
#include <unistd.h>  

#include <sys/time.h>

#include <errno.h>
#include <assert.h>

#include "relay_comms.h"
#include "header_parser.h"
#include "error_codes.h"
#include "defaults.h"



struct timeval read_timeout =
  {.tv_sec = READ_TIMEOUT_SEC, .tv_usec = READ_TIMEOUT_USEC};


struct header_data {
  char header_storage[MAX_HEADER_LENGTH];
  int amount_stored;
  struct http_header_info info;
};


/**************************** Prototypes ********************************/

int relay_response(int RX_socket, int TX_socket, struct rate *rate_limit);

int relay_request(int RX_socket, int TX_socket, char *host_field, 
                  struct header_data *request_header, struct rate *rate_limit);


int time_limit_read(int RX_socket,  char *buffer, int buffer_size, 
		    struct timeval *timeout);

int read_in_header(struct header_data *header, int reading_socket, 
		   struct timeval *timeout);

int read_header(struct header_data *header, int reading_socket,
		struct timeval *timeout);

int send_msg(struct header_data *header, int msg_length, 
             int read_socket, int send_socket, struct rate *rate_limit);

int send_rate_limited(int TX_socket, char *message, int size_message, 
                      struct rate *rate_limit);

int rate_limited_relay(int RX_sock, int TX_sock, int amount2relay,
                       struct rate *rate_limit);




int max(int number1, int number2);

void remove_message(struct header_data *header, char *message_end);

// Testing functions 
//void test_read_in_header(void);
void test_remove_message(void);
void test_time_limit_read(void);
void test_send_msg(void);

/***********************************************************************/

/* Relays HTTP information between client and the host given by the client in
 * The HTTP request. Rate limiting is applied if given .conf file otherwise
 * the default rate limiting will be applied.
 *
 * client_socket -> The socket connection between client and proxy
 * config_options -> The options from the parsed .conf file. See config.h
 *
 * Return:
 *       1 Success
 *      -1 Error has occurred
 *      -HTTP_STATUS_CODE - Error in parsing the http request
 */
int relay(int client_socket,
		struct config_sect * config_options,
		int rate_limiting)
{
  assert(client_socket >= 0);

  int status;
  int server_socket;
  char host_field[MAX_URL_SIZE];

  size_t client_msg_length;

  fd_set readfds, masterfds; 
  int max_file_desc;
 
  struct header_data client_header;
  client_header.amount_stored = 0;
  
  struct rate *rate_limit_ptr = NULL;
  // server to client timers


  // Read in first header - keep reading until get a valid header field
  do { 
    status = read_header(&client_header, client_socket, &read_timeout);
    if(status <= 0 && status != BAD_REQUEST) return status;
  }while(status == BAD_REQUEST);

  // Get the host name 
  status = get_host(&(client_header.info), host_field, sizeof(host_field));
  printf("Host: %s\n", host_field);
  if(status < 0) return status; // invalid host field


  // Get the rate limit from .conf file
  struct rate rate_limit = { .period = {.tv_sec = 1, .tv_usec = 0}};
  rate_limit.bin_max_amount =
      convertToBpInterval(get_rate_limit(config_options, host_field));
  rate_limit.bin_amount = rate_limit.bin_max_amount;
  

  if(gettimeofday(&(rate_limit.timestamp), NULL) <= -1){  // Fix this
      printf("ERROR getting time");
      return -1;
    }

  // Check whether we are implementing rate limiting
  if(rate_limiting){
    rate_limit_ptr= &rate_limit;
  }
  
//  printf("Rate limiting %s to %dB/s\n", host_field, rate_limit.bin_max_amount );
    
  // Get the content length of the body
  client_msg_length = get_content_length(&(client_header.info));
  if(client_msg_length < 0) return client_msg_length; 

  // Set up server socket
  server_socket = setup_socket(SERVER_PORT, host_field);
  if(server_socket < 0) return server_socket;
//   printf("Setup server socket\n");

  status = send_msg(&client_header, client_msg_length, 
		    client_socket, server_socket, NULL);
  if(status < 0){
    close(server_socket);
    return status;
  }

  // Set up the select statement
  FD_ZERO(&masterfds); 
  FD_SET(server_socket, &masterfds); 
  FD_SET(client_socket, &masterfds); 
  
  max_file_desc = max(server_socket, client_socket);

  while(1){       
    // reset select
    FD_ZERO(&readfds);
    memcpy(&readfds, &masterfds, sizeof(readfds));

    // Find a socket which is not blocked 
    if(select(max_file_desc+1, &readfds, NULL, NULL, &read_timeout) == -1)
      {     
	printf("\nError occured in select: %s", strerror(errno));
	close(server_socket);
	return -1;
      }
    
    // Relay CLIENT -> SERVER
    if(FD_ISSET(client_socket, &readfds)){
      // Do not rate limit from client to server
      status = relay_request(client_socket, server_socket, host_field, 
			     &client_header, NULL);

      // if the read connection is closed exit
      if(status == 0) {
	close(server_socket);
	return 1;
      }
      else if(status == BAD_REQUEST) continue; // Yet to find header - try again
      else if(status < 0){
	close(server_socket);
	return status;
      }
    }

    // Relay SERVER -> CLIENT
    if(FD_ISSET(server_socket, &readfds)){
      status = relay_response(server_socket, client_socket, rate_limit_ptr);
//    	status = relay_response(server_socket, client_socket, &rate_limit);

      if(status == 0) {
	close(server_socket);
	return 1; // if the read connection is closed exit
      }
      else if(status < 0){
	close(server_socket);
	return status;
      }
    } 
  }
} // End relay



/* Relays the request header and the body (if it exists) to the server.
 * No rate rate limiting is applied for the client request
 *
 * Return:
 *      1 Success
 *      0 if connection either RX_socket or TX_socket have been closed
 *      <=0 if error occurred (see error_codes.h if <= -400)
 */
int relay_request(int RX_socket, int TX_socket, char *host_field, 
		  struct header_data *request_header, struct rate *rate_limit){

  assert(host_field != NULL);
  assert(request_header != NULL);
  assert(RX_socket >= 0 && TX_socket >= 0);
 
  char requested_host[MAX_URL_SIZE];

  int status = read_header(request_header, RX_socket, NULL);  
  if(status <= 0) return status;

  status = get_host(&(request_header -> info), requested_host, sizeof(host_field));
  if(status < 0) return status; // invalid host field

  // if the host field is different 
  if(strcmp(host_field, requested_host) != 0) return -1; // close the connection
    
  int client_msg_length = get_content_length(&(request_header -> info));
  if(client_msg_length < 0) return client_msg_length; 

  status = send_msg(request_header, client_msg_length, 
		    RX_socket, TX_socket, rate_limit);
  if(status <= 0) return status;
  return 1;
} // End relay_request



/* Relays any information from the server back to the client
 * Applies rate limiting if bin_amount, init_time, max_amount and interval are
 * all set. Otherwise no rate limiting will be applied
 */
int relay_response(int RX_socket, int TX_socket, struct rate *rate_limit){

  int message_size = RELAY_BUF_SIZE;
 
  if(rate_limit != NULL) message_size = rate_limit -> bin_max_amount;

  char message[message_size];
  memset(message, 0, message_size);

  int nread = read(RX_socket, message, message_size);
  if (nread < 0){
    printf("ERROR in reading:%s",strerror(errno)); 
    return -1;
  }
  if(nread == 0) return 0;

  return send_rate_limited(TX_socket, message, nread, rate_limit);
} // End relay_response



/* Read data given a specific timeout. If no read has occurred before timeout
 * return REQUEST_TIMEOUT.
 *
 * If timeout is NULL then no time limit will be imposed in reading from socket
 *
 * Return REQUEST_TIMEOUT if read timed out
 *  -1 for read error or select error
 *   0 if the connection was closed
 *  >0 number of bytes read in
 */
int time_limit_read(int RX_socket,  char *buffer, int buffer_size, 
		    struct timeval *timeout)
{
  assert(buffer_size > 0);
  assert(buffer != NULL);
  
  if(timeout != NULL){
    fd_set readfds;
    FD_ZERO(&readfds); 
    FD_SET(RX_socket, &readfds);
 
    if(select(RX_socket + 1, &readfds, NULL, 
	      NULL, timeout) == -1)
      {
	printf("\nError occured in select: %s", strerror(errno));
	return -1; 
      }
	
    // if the header could not be read within timeout throw REQUEST_TIMEOUT error
    if(FD_ISSET(RX_socket, &readfds) != 1) return REQUEST_TIMEOUT;
  }

  return read(RX_socket, buffer, buffer_size);
} // End time_limit_read



/* Read in the HTTP header.
 *
 * return REQUEST_ENT_TOO_LARGE if header is too large.
 *        BAD_REQUEST if the entire header can not be read in
 *        -1 if timed out.
 *         0  reading socket was closed - could not read header
 *         1 success header information has been read in and is stored in the
 *           header_data struct
*/
int read_in_header(struct header_data *header, int reading_socket, 
		   struct timeval *timeout){
  // if the header data is already full and has not found end of header
  if(header -> amount_stored >= sizeof(header -> header_storage)){ 
    return REQUEST_ENT_TOO_LARGE;
  }

  int read_in_amount = 
    sizeof(header -> header_storage) - (header -> amount_stored);
  
  char *read_location = 
    (header -> header_storage) + (header -> amount_stored);
 
  int read_status = time_limit_read(reading_socket, read_location,
				    read_in_amount, timeout);

  if(read_status <= 0) return read_status;
  else{
    header -> amount_stored += read_status;
  }
  return 1;
} // End read_in_header



/* Reads in header and tries to parse it, storing the parsed information in
 * header_data struct.
 * return
 *      BAD_REQUEST -> Have not received full header wait for more information
 */ 
int read_header(struct header_data *header, int reading_socket, 
		struct timeval *timeout) 
{						 
  assert(header != NULL);
  assert(header -> amount_stored >= 0);
  assert(reading_socket >= 0);
  
  int parse_status;
  
  //while( parse_status == BAD_REQUEST || header -> amount_stored == 0 ){

  int read_status = 
    read_in_header(header, reading_socket, timeout);	 
  if(read_status <= 0) return read_status;

  parse_status = parse_header(&(header -> info), 
			      header -> header_storage, 
			      header -> amount_stored);
  //}

  // check that the parsing was correct if not then return value
  if(parse_status < 0) return parse_status;
  else return 1;
} // End read_header



/* Send both header and body of the message. If the content-length is 0 or does
 * not exist then only the header will be sent. Can be rate-limited.
 *
 * Return  1 on success
 *   0 if the send_socket was closed after sending.
 *   <=-1 an error occured sending or relaying message
*/
int send_msg(struct header_data *header, int msg_length, 
	     int read_socket, int send_socket, struct rate *rate_limit)
{

  int header_length =
    header -> info.header_end - (header -> info.read_storage) + 1;
  int amount2send = header_length + msg_length;

  if(amount2send <= header -> amount_stored){
    // Send message
    if( send_rate_limited(send_socket, header -> header_storage,
                          amount2send, rate_limit) < 0)
      {
	header -> amount_stored = 0;
	return -1; // error in sending
      }
	
    char *msg_end = header -> header_storage + amount2send-1;
    //remove old message
    remove_message(header, msg_end);
  }
  else{
    //send Message
    if(send_rate_limited(send_socket, header -> header_storage, 
			 header -> amount_stored, rate_limit) <= 0)
      {
	header -> amount_stored = 0;
	return -1; // error in sending
      }
	 
    amount2send = amount2send - header -> amount_stored;
	 
    int send_status =
        rate_limited_relay(read_socket, send_socket, amount2send, rate_limit);
    if(send_status <= 0){
      header -> amount_stored = 0;
      return send_status; // error in sending
    }
    // remove old message
    header -> amount_stored = 0;
  }
  return 1;
} // End send_msg



/*Send at a limited rate 
  Return the amount of data that has been sent during this time interval
  as well as returning the start of the interval

  if connection closed then return 0
  Other sending errors return -1
*/
int send_rate_limited(int TX_socket, char *message, int size_message, 
		      struct rate *rate_limit)
{

  assert(message != NULL);
  assert(size_message >= 0);
  assert(TX_socket > 0);
  
  int rate_limited = 0; // is it rate limited
  if(rate_limit != NULL){
      printf("Sending data rate limited to %dB/s\n", rate_limit -> bin_max_amount);
      rate_limited = 1;
  }

  int nwrite = 0;
  int amount_written = 0;
  int amount2write = size_message ;
  
  while(amount2write > 0){
    // Suspends if amount is empty
    if(rate_limited){
      suspend(rate_limit);
      if(rate_limit -> bin_amount < amount2write){
        nwrite = write(TX_socket, message + amount_written, rate_limit -> bin_amount);
      }
      else{
        nwrite = write(TX_socket, message + amount_written, amount2write);
      }
      update_bin(nwrite, rate_limit);
    }
    else{
      nwrite = write(TX_socket, message + amount_written, amount2write);
    }
	
    amount2write = amount2write - nwrite;
    amount_written = amount_written + nwrite;
 
    if(nwrite < 0 || (nwrite == 0 && amount2write > 0)){ 
      printf("Error writing:%s", strerror(errno));
      return -1;
    }
    else if(nwrite == 0) return 0;
  }
  return 1;
} // End send_rate_limited



/* Relay an amount at a certain speed. If non of the rate limiting parameters
 * are given then no rate limiting will be applied.
 *
 */
int rate_limited_relay(int RX_sock, int TX_sock, int amount2relay,
		       struct rate *rate_limit)
{
  assert(amount2relay >= 0);
  assert(RX_sock >= 0);
  assert(TX_sock >= 0);
  
  int rate_limiting  = 1;
  if(rate_limit == NULL) rate_limiting = 0;


  int nread = 0;
  int amount_read = 0;
  int message_size;
  while(amount_read < amount2relay){

    if(rate_limiting) message_size = rate_limit -> bin_amount;
    else	message_size = RELAY_BUF_SIZE;

    char message[message_size];
    memset(message, 0, sizeof(message));
    nread = time_limit_read(RX_sock, message, 
			    sizeof(message), &read_timeout);

    if (nread == REQUEST_TIMEOUT) {
      printf("ERROR Read timed out");
      return nread;
    }
    else if (nread <= -1) {
      printf("ERROR in reading:%s",strerror(errno)); 
      return nread;
    }
    else if( nread == 0){
      printf("ERROR connection closed before finished reading");
      return -1;
    }

    amount_read = nread + amount_read;
    int send_status = send_rate_limited(TX_sock, message, nread, rate_limit);
    if(send_status < 0)return -1;
    if((send_status == 0) && (amount_read < amount2relay)){
      printf("ERROR closed connection before could send every thing");
      return -1;
    }
    else if(send_status == 0) return 0;
  }
  return 1;
} // End rate_limited_relay



/* Sets up a listening connection if Host == NULL -> suitable for a server
   else sets up a direct connection suitable for a client

   Returns: Socket if successful
			-1 if an error in creating the socket has occurred
 */
int setup_socket(char * port, char * host){
  int n, sock;//, sock_out;
  struct addrinfo hints, *res, *rp;
 
  memset(&hints, 0, sizeof (hints));
    
  hints.ai_family = AF_UNSPEC;    // or AF_INET6 ... or  AF_UNSPEC
  hints.ai_socktype = SOCK_STREAM;      // for tcp 

  // Set port to listen if server
  if(host == NULL){
//     fprintf(stdout, "Creating Client Socket\n");
    hints.ai_flags = AI_PASSIVE;  // wildcard suitable for server 
  } else { 
//     fprintf(stdout, "Creating Server Socket\n");
    fprintf(stdout, "Host: %s\n", host);
    hints.ai_flags = 0;
  }
    
  // Find Internet address
  if ((n = getaddrinfo (host, port, &hints, &res))) {
    printf("ERROR in getaddrinfo: %s", strerror(errno));
    return -1; 
  }

  if(host == NULL){
    for (rp = res; rp != NULL; rp = rp->ai_next) {
      // Setup socket 
      sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sock == -1) continue;

      // set so can reuse socket
      int yes = 1;
      setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));

      // Bind the socket to listening socket
      if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)	break;  // Success 

      close(sock);  // Failed to find successful socket
    }

    if (rp == NULL) { // No address succeeded 
      fprintf(stderr, "Could not bind\n");
      return -1;
    }
  
    // Set to listen on this port
    if (listen(sock, MAX_QUEUE) < 0 ){
      printf("ERROR in listening to sock: %s", strerror(errno));
      return -1;
    }
  } 
  else {      
    for (rp = res; rp != NULL; rp = rp->ai_next) {
      // Setup socket 
      sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sock == -1) continue;

      // Bind the socket to listening socket
      if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) break; // Success 

      // Failed to find successful socket
      close(sock);
    }

    if (rp == NULL) {        // No address succeeded 
      fprintf(stderr, "Could not connect to host\n");
      exit(EXIT_FAILURE);
    }
  }
  freeaddrinfo(res); // free the link list
  return sock;
} // End setup_socket


// Return the larger of two numbers
int max(int number1, int number2){
  if(number1 > number2){
    return number1;
  }
  else return number2;
} // End max



/* Remove the first message from the header. The end of the message is
 * determined by message_end
 */
void remove_message(struct header_data *header, char *message_end){

  assert(header != NULL);
  if(header -> amount_stored == 0) return;

  char *end_data = (header -> header_storage) + (header -> amount_stored) -1;
  assert(message_end <= end_data); 

  if(message_end == end_data){
    header -> amount_stored = 0;
    return;
  }

  char *next_message = message_end + 1;
  int size_rest_data = end_data - message_end;

  memmove(header -> header_storage, next_message, size_rest_data);
  header -> amount_stored = size_rest_data;
} // End remove_message





/***********************************TESTS *****************************/

void relay_tests(void){
  // test_read_int_header();
  // test_remove_message();
  // test_time_limit_read();
  test_send_msg();
} // End relay_tests


void test_send_msg(void){
  int read_status;
  
  struct header_data header = {.amount_stored = 0};

  // Testing HTTP_request_short.txt
  // printf("\n\n **** Test HTTP_request_short.txt ***\n");
  int file_read = open("./test_files/HTTP_Request_short.txt", O_RDONLY);
  if(file_read == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }

  int file_write = open("./test_files/temp_write.txt", O_WRONLY | O_CREAT);
  if(file_write == -1){
    printf("Error Occured opening temp write file: %s", strerror(errno));
    return;
  }

  read_status = read_header(&header, file_read, NULL);
  if(read_status == -1){
    printf("\nFAILED: %s\n", strerror(errno)); 
  }
 
  int content_length = get_content_length(&(header.info));
  /* if(content_length == 0){
     printf("\nSUCCESS : correct content length");
     }
     else printf("\nFAIL : incorrect content length (0) got:%d",
     content_length);
  */ 
  send_msg(&header, content_length, file_read, 
			  file_write, NULL);

  close(file_read);
  close(file_write);
  
  char buffer[100000]; 
  file_write = open("./test_files/temp_write.txt", O_RDONLY);
  read(file_write, buffer, sizeof(buffer));
  // printf("\n\nReceived Output:");
  //  printf("\n%s..............\n", buffer);
  close(file_write);


  //  Test HTTP_reponse.txt 
  //  printf("\n\n **** Test HTTP_reponse.txt ***\n");
  file_read = open("./test_files/HTTP_Response3.txt", O_RDONLY);
  if(file_read == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }

  file_write = open("./test_files/temp_write.txt", O_WRONLY | O_CREAT);
  if(file_write == -1){
    printf("Error Occured opening temp write file: %s", strerror(errno));
    return;
  }

  read_status = read_header(&header, file_read, NULL);
  if(read_status == -1){
    printf("\nFAILED: %s\n", strerror(errno)); 
  }
 
  content_length = get_content_length(&(header.info));
  /*if(content_length == 282){
    printf("\nSUCCESS : correct content length");
    }
    else printf("\nFAIL : incorrect content length (282) got:%d",
    content_length);
  */
  send_msg(&header, content_length, file_read,file_write, NULL);

  close(file_read);
  close(file_write);
  
  file_write = open("./test_files/temp_write.txt", O_RDONLY);
  read(file_write, buffer, sizeof(buffer));
  //printf("\n\nReceived Output:");
  // printf("\n%s............\n", buffer);
  printf("%s", buffer);
  close(file_write);
} // End test_send_msg


void test_remove_message(void){
  struct http_header_info header_info;

  struct header_data header = {"hello world", 11, header_info};

  remove_message(&header, header.header_storage + 6);
  printf("\nExpect: orldo world");
  printf("\nGot:    %s", header.header_storage);
  printf("\namount(4):%d\n", header.amount_stored);

  header = (struct header_data) {"hello world", 7, header_info};
  remove_message(&header, header.header_storage + 6);
  printf("\nExpect: hello world");
  printf("\nGot:    %s", header.header_storage);
  printf("\namount(0):%d\n", header.amount_stored);
} // End test_remove_message


void test_time_limit_read(void){

  int read_status;
  
  char RX_buffer[8012];

  struct timeval timeout = {2, 0};
  // Testing HTTP_request_short.txt
  printf("\n\n **** Test HTTP_request_short.txt ***\n");
  int file_desc = open("./test_files/HTTP_Request_short.txt", O_RDONLY);

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }

  read_status = time_limit_read(file_desc, RX_buffer, sizeof(RX_buffer),
				&timeout);
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  printf("\n\n%s\n", RX_buffer);
  close(file_desc);
} // End test_time_limit_read



