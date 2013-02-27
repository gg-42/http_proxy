/******************************** header_parser.c **************************
 Description: 
  This contains functions to read and parse a http header message.
             
 Author(s): Geoffrey Goldstraw (u4528129)        

 Version:          $Rev: 55 $
 Date Modified:    $LastChangedDate: 2012-05-25 15:06:14 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4528129 $ 

***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "defaults.h"
#include "header_parser.h"
#include "error_codes.h"

//Testing
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



/************************ Prototypes ***************************/
int tag_header(struct http_header_info *http_header);
char* find_field_end(char *start_pos, char *end_pos );

int get_field(struct http_header_info *http_header, 
	      char *field_name, 
	      char *url_storage, 
	      int sizeof_url_storage);

int get_field_content(char *content_start_pos,
		      char *content_end_pos,
		      char *content_storage,
		      int  sizeof_storage);

int is_field(char *field_name, 
	     char *field_start_pos, 
	     char *field_end_pos,
	     char **content_start_pos);

char* find_non_CRLF(char *start_pos, char * end_pos);
int is_CRLF(char *pos, char *end_pos);
char* find_non_whitespace(char *start, char *end);


// Testing functions
void test_tag_header(void);
void test_find_field_end(void);
void test_find_non_CRLF(void);
void test_is_CRLF(void);
void test_find_non_whitespace(void);
void test_get_field_content(void);
void test_is_field(void);
void test_get_field(void);
void test_get_content_length(void);
/***************************************************************/


/* Reads in the http header so other methods can access various header
   fields.

   It Ignoring empty CRLF lines above the "request" line.
   (See http spec 4.1)

   Checks for continuing header fields HTTP specification - 4.2

   Return: The size of the header field
   Errors
   REQUEST_ENT_TOO_LARGE - Too many headers fields, raise Entity too large

   BAD_REQUEST - Header has no length or does not have a double carriage return
   at the end of the sequence.
   Raise a http 400 error (Bad Request)

   -1  received_data is NULL or invalid size
*/
int parse_header(struct http_header_info *http_header, char *received_data,
		 int sizeof_RX_data){
  assert(http_header != NULL);
  if(received_data == NULL || sizeof_RX_data <= 0) return -1;

  http_header->read_storage = received_data;
  http_header->end_data = http_header->read_storage + sizeof_RX_data -1;

  return tag_header(http_header);
}



/*Goes through the http header and finds the content stored in the 
  host header field. If url_storage is not large enough it will 
  return with an error -1

  Precondition: The HTTP header must be brought into memory beforehand
  using the procedure parse_header()

  Return 0 if failed to find a host
  -1 if error occurred
  positive integer indicates the size stored
*/
int get_host(struct http_header_info *http_header, char *url_storage, 
	     int sizeof_url_storage){
  // assert(http_header != NULL);
  char *host = "Host";
  return get_field(http_header, host, url_storage, sizeof_url_storage);
}



/* Finds gets the content-length field
   Return  Length of the body
     -1 if error has occurred */
int get_content_length(struct http_header_info *http_header){
  assert(http_header != NULL);
 
  char *field = "Content-Length";
  char field_val[MAX_CONTENT_LENGTH_DIGITS];
  int field_status = get_field(http_header, field, 
			       field_val, sizeof(field_val));

  if(field_status < 0 )	 return -1; // length required

  // No content length could be found - the Content-Length = 0
  else if(field_status == 0) return 0; 
  else{
    long value = strtol(field_val, NULL , 10);
    if (value < 0){
      printf("ERROR in converting Content-length size: %s", 
	     strerror(errno));
      return -1;
    }
    else {
      return value;
    }
  }
}



/*Goes through the http header and finds the content stored in the 
  specifid header field. If url_storage is not large enough it will 
  return with an error -1

  Precondition: The Http header must be brought into memory beforehand
  using the produre parse_header()

  Return  0 if failed to find a field
  -1 if error occured
  positive integer indicates the size stored
*/
int get_field(struct http_header_info *http_header, 
	      char *field_name, 
	      char *url_storage, 
	      int sizeof_url_storage)
{
  assert(http_header != NULL);

  if(sizeof_url_storage <= 0) return -1;
  // there are no fields to search
  if(http_header->num_fields < 0) return -1;
  if(http_header->num_fields == 0) return 0;

  assert(http_header->header_end <= http_header->end_data);
  assert(http_header->read_storage <= http_header->header_end);

  int i;
  int field_status;
  char *content_start_pos = NULL;
  char *field_end_pos;
  
  /* skip the first field as this is the request field and does not
     match the syntax of other header fields (see http spec)*/
  for(i = 1; i < http_header->num_fields ; i++){

    if(i == http_header->num_fields -1){
      field_end_pos = http_header->header_end;
    }
    else{
      field_end_pos = (http_header->header_fields[i+1]) -1;
    }

    field_status = is_field(field_name, http_header->header_fields[i], 
			    field_end_pos, &content_start_pos);

    if(field_status == 1){
      int size = get_field_content(content_start_pos, field_end_pos, 
				   url_storage, sizeof_url_storage);
      if(size <= 0){
	return -1;
      }
      else return size;
    }
    if( field_status <= -1 ) return -1; 
  }
  return 0;  // Failed to find field 
}


 
/* Given a field name, goes through the fields in the http header
   and tries to find a field that corresponds to field name. 
   Will assign pointer to the start of content (content_start_pos)
	
   Note: fields names are case insensitive (http spec 4.2)
	
   Return pointer to start of the content field if successful
   0 if the field was not found
   -1 if error occurred
*/ 
int is_field(char *field_name, 
	     char *field_start_pos, 
	     char *field_end_pos,
	     char **content_start_pos)				 
{
  assert(field_name != NULL || field_start_pos != NULL || 
	 field_end_pos != NULL);

  assert(field_start_pos <= field_end_pos);

  assert(*field_name != ' ');
  
  //assumed that the field start position is well formed and points directly
  // to starting location
  int field_name_len = strlen(field_name); 

  if(field_name_len > (field_end_pos - field_start_pos) +1) 
    return 0; // does not match as the string comparing is larger
  else{
    int i;
    for(i = 0; i <= field_name_len -1; i++){
      if(tolower(field_name[i]) != tolower(field_start_pos[i])) return 0;
    } 

    // Check to see that next non white space character is colon ':'  
    if(field_start_pos + field_name_len < field_end_pos){
      char *non_white_space = 
	find_non_whitespace(field_start_pos + field_name_len,
			    field_end_pos);
			
      if(non_white_space == NULL)  return -1;

      else if( *non_white_space == ':'){
	if(non_white_space < field_end_pos){
	  *content_start_pos =  non_white_space + 1;
	  return 1;
	}
	else return -1;
      } 
      else return -1;
    }
    else return -1;
  }
}



/* Get the content of the header field. 
   This ignores any white space before and after the content.

   Returns the size of the url
   -1 if it does not conform (malformed header)*/
int get_field_content(char *content_start_pos,
		      char *content_end_pos,
		      char *content_storage,
		      int  sizeof_storage){
 
  assert( content_start_pos != NULL || content_end_pos != NULL);
  assert( content_storage != NULL );
  assert( content_storage > 0 );
    
  char *content_start = find_non_whitespace(content_start_pos, 
					    content_end_pos);

  if(content_start == NULL) return -1; // No content field just white space

  int size = 0;
  int CRLF = 0;
  int CR = 0;
 
  char *i;
  for(i = content_start; i <= content_end_pos; i++){
    if(size > sizeof_storage-1) return -1;
    else if( (size == sizeof_storage && !CR) 
	     || (i == content_end_pos && !CR) ) return -1;

    else if( *i == '\r' && !CR) {
      CR = 1; 
    }
    else if ( *i == '\r') return -1;
  
    else if ( *i == '\n' && CR){
      CR = 0;
      CRLF = 1;
    }    
    else if ( *i == '\n') return -1;   
  
    else if( *i != ' '){
      *(content_storage + size) = *i;
      size++;
    }
    else if( *i == ' ' && ! CRLF){
      *(content_storage + size) = '\0' ; // put the null character in
      size++;
      return size;
    }
    else if( *i == ' ' && CRLF) {
      CRLF = 0;
    }
  }
  *(content_storage + size) = '\0' ; // put the null character in
  size++;
  return size;
}



/* Goes through a sequence of character looking for the first non white
   space character 

   Returns pointer to first non white space character
   NULL if no non white space character could be found
*/
char* find_non_whitespace(char *start, char *end){
  assert(start != NULL || end != NULL); 
  assert(start <= end);
  
  char *i;
  for(i = start; i <= end; i++){
    if(*i != ' ') return i; 
  } 
  // Could not find a non white space character
  return NULL;
}



/* Prints the header field
   Indexing starts at 0

   Return 1 on success
   -1 on if invalid number
*/
int print_header_field(struct http_header_info *http_header, int field_num){
  assert(http_header != NULL);

  if(field_num  > http_header->num_fields -1 
     || http_header->num_fields == 0){
    return -1; // Invalid field number
  }

  char *start_of_field, *end_of_field;

  start_of_field =  http_header->header_fields[field_num];
 
  // If printing the last field in the header
  if(field_num == http_header->num_fields -1){
    end_of_field = http_header->header_end;
  }
  else {
    end_of_field = (http_header->header_fields[field_num +1]) - 1;
  }
  print_range(start_of_field, end_of_field);
  
  return 1;
}



/* Prints all the header fields */
void print_header(struct http_header_info *http_header){
  assert(http_header != NULL);
  int i;
  for(i = 0; i < http_header->num_fields; i++){
    print_header_field(http_header, i);
  } 
}



/*
  Goes through and tags the http header at points where a new header field
  starts

  It ignoring empty CRLF lines above request (http spec 4.1)
  Checks for continuing header fields HTTP specification - 4.2 

  Return: The size of the header field
  Errors
  REQUEST_ENT_TOO_LARGE -  Too many headers, raise entity too large
 
  BAD_REQUEST - Header has no length or does not have a double carriage return
  at the end of the sequence. 
  Raise a http 400 error (Bad Request)   
*/
int tag_header(struct http_header_info *http_header){
  assert(http_header != NULL);
  // Ignoring empty CRLF lines above request (http spec 4.1)
  http_header->header_fields[0] = find_non_CRLF(http_header->read_storage, 
						http_header->end_data);
  if(http_header->header_fields[0] == NULL){
    http_header->num_fields = 0;
    return BAD_REQUEST; // header has no length or does not have a valid \r\n
  }
  http_header->num_fields = 1;
  
  char *end_field_pos = find_field_end(http_header->read_storage,
				       http_header->end_data);
    
  int i = http_header->num_fields;
  int CRLF_status;
  while(i < MAX_NUM_FIELDS){
    
    if (end_field_pos == NULL){
      http_header->num_fields = 0;
      return BAD_REQUEST; 
    }

    // check if there is any other header fields
    CRLF_status = is_CRLF(end_field_pos +1 , http_header->end_data);
     
    //if no other header fields but invalid ending 
    if(CRLF_status == -1){
      http_header->num_fields = 0;
      return BAD_REQUEST;
    }
    // If reached the end of the header 
    if(CRLF_status == 1){    
      // header_end now points to the LF of the second CRLF
      http_header->header_end = end_field_pos + 2; 
      return http_header->header_end - http_header->header_fields[0] + 1;
    }

    // Other header fields exits store the header start
    else {
      // Check for continuing header fields HTTP specification - 4.2 
      // As we already know end_data is larger than (end_field_pos +1) 
      if(*(end_field_pos +1) != ' '){
	http_header->header_fields[i] = end_field_pos +1;
	http_header->num_fields = i+1;
	i++;
      }
    }

    // Go to the end of the field to find the next field header
    end_field_pos = find_field_end(end_field_pos +1,
				   http_header->end_data);  
  }// end while
  
  //Reach the maximum number of fields - Raise "Entity too large"
  http_header->num_fields = 0; // set to the 0 
  return REQUEST_ENT_TOO_LARGE;
}




/* Prints all the characters between and including the specified 
   range. */
void print_range(char *start, char *end){
  char *i = start;
  while (i  <= end){
    printf("%c", *i);
    i++;
  }
}



/*Goes through a string defined by two pointers and finds the "\r\n"
  sequence. 
  Returns: A pointer to the \n character in the sequence "\r\n"
  NULL if the sequence "\r\n" could not be found
*/
char* find_field_end(char *start_pos, char *end_pos ){
  assert(start_pos != NULL || end_pos != NULL);
  assert(start_pos <= end_pos);

  if(start_pos == end_pos){
    return NULL;
  }

  char *prev = start_pos;

  char *index;
  for(index = start_pos +1; index <= end_pos; index++){

    if(*prev == '\r' && *index == '\n'){
      return index;
    }
    prev = index;
  }

  return NULL;
}



/* Finds the first non CRLF character
   Return NULL if no CRLF could be found
*/
char* find_non_CRLF(char *start_pos, char * end_pos){
  assert(start_pos != NULL || end_pos != NULL);
  assert(start_pos <= end_pos);

  if(start_pos == end_pos){
    return NULL;
  }

  char *index, *prev;
  for(index = start_pos +1; index < end_pos ; index = index +2){
  
    prev = index -1 ;
    
    if(*prev != '\r' && *index != '\n'){
      return prev;
    }
  } 
  return NULL;
} 



/*Check if pos was pointing to start of a CRLF sequence.

  Return:  1 - pos is pointing to a CRLF sequence
  -1 - ERROR, pos is larger than the string 
*/
int is_CRLF(char *pos, char *end_pos){
  if (pos >= end_pos) return -1;

  if(*pos == '\r' && *(pos+1) == '\n')  return 1;
  else return 0;
}




/*****************************TESTING FUNCTIONS**************************/

void header_parser_tests(void){
  
  printf("\n\n Testing find_non_CRLF");
  test_find_non_CRLF();

  printf("\n\n Testing find_field_end");
  test_find_field_end();

  printf("\n\n Testing is_CRLF");
  test_is_CRLF();
  

  printf("\n\n Testing tag_header()");
  test_tag_header();

  printf("\n\n*** Test find_non_whitespace ***\n");
  test_find_non_whitespace();

  printf("\n\n*** Test get_field_content ***\n");
  test_get_field_content();

  printf("\n\n*** Test is_field ***\n");
  test_is_field();
 
  printf("\n\n*** Test get_field ***\n");
  test_get_field();

  printf("\n\n*** Test get_content_length ***\n");
  test_get_content_length();
  
}

void test_get_content_length(void){
  char read_buffer[8012];
  int read_status;
  struct http_header_info http_header;

  // Testing HTTP_request_short.txt
  printf("\n\n **** Test HTTP_request_short.txt ***\n");
  int file_desc = open("./test_files/HTTP_Request_short.txt", O_RDONLY);

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }
 
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  int value;
  parse_header(&http_header, read_buffer, read_status);
  value = get_content_length(&http_header);

  printf("\nExpected: 0\n");
  printf("Result:   %d\n",value );
  close(file_desc);

  // Testing HTTP_request_short.txt
  printf("\n\n **** Test HTTP_Response.txt ***\n");
  file_desc = open("./test_files/HTTP_Response.txt", O_RDONLY);

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }
 
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  parse_header(&http_header, read_buffer, read_status);
  value = get_content_length(&http_header);

  printf("\nExpected: 282\n");
  printf("Result:   %d\n",value );
  close(file_desc);

  print_header(&http_header);

  printf("\n\n field 10\n");
  print_header_field(&http_header,9); 

}


void test_get_field(void){
  char read_buffer[8012];
  int read_status;
  char storage[100];
  struct http_header_info http_header;
  // Testing HTTP_request_short.txt
  printf("\n\n **** Test HTTP_request_short.txt ***\n");
  int file_desc = open("./test_files/HTTP_Request_short.txt", O_RDONLY);

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }
 
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  parse_header(&http_header,read_buffer, read_status);
  get_field(&http_header,"host", storage, sizeof(storage));

  printf("\nExpected: dwl999.blogspot.com.au\n");
  printf("Result:   %s\n", storage);
  close(file_desc);

  // HTTP_request.txt test
  printf("\n\n *** Test HTTP_request.txt *** \n");
  file_desc = open("./test_files/HTTP_Request.txt", O_RDONLY);
  
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  parse_header(&http_header,read_buffer, read_status);
  get_field(&http_header,"host", storage, sizeof(storage));

  printf("\nExpected: dwl999.blogspot.com.au\n");
  printf("Result:   %s\n", storage);

  get_field(&http_header,"ACCEPT", storage, sizeof(storage));

  printf("\nExpected: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\n");
  printf("Result:   %s\n", storage);

  get_field(&http_header, "Cookie", storage, sizeof(storage));
  printf("\nExpected: blogger_TID=4bd1206bb13d9c17\n");
  printf("Result:   %s\n", storage);

  close(file_desc);
}



void test_is_field(void){

  //Test normal no spaces or capitals
  printf("\n Test normal no spaces or capitals\n");
  char *test_string = "host:This_is_test_content\r\n";
  char *content_start = NULL;
  char result[100];
  char *field_end = find_field_end(test_string,
				   test_string + strlen(test_string) -1);
  
  int field_status =  
    is_field("host", test_string, field_end, &content_start);
 
  printf("\nExpected: 1\n" ); 
  printf("Result: %d\n", field_status);

  get_field_content(content_start, field_end, result, sizeof(result));
  printf("content: \"%s\"\n", result);


  //Test normal with  spaces and Capitals
  printf("\n Test normal with  spaces and Capitals\n");
  test_string = "Host  :  This_is_test_content\r\n";
  field_end = find_field_end(test_string,
			     test_string + strlen(test_string) -1);
  
  field_status = is_field("host", test_string, field_end, &content_start);
 
  printf("\nExpected: 1\n" ); 
  printf("Result: %d\n", field_status);

  get_field_content(content_start, field_end, result, sizeof(result));
  printf("content: \"%s\"\n", result);


  //Test no colon
  printf("\n Test no colon \n");
  test_string = "host  This_is_test_content\r\n";
  field_end = find_field_end(test_string,
			     test_string + strlen(test_string) -1);
  
  field_status = is_field("host", test_string, field_end, &content_start);
 
  printf("\nExpected: -1\n" ); 
  printf("Result: %d\n", field_status);


  //Test no match
  printf("\n Test no match \n");
  test_string = "GET : This_is_test_content\r\n";
  field_end = find_field_end(test_string,
			     test_string + strlen(test_string) -1);
    
  field_status = is_field("host", test_string, field_end, &content_start);
 
  printf("\nExpected: 0\n" ); 
  printf("Result: %d\n", field_status);

  //Test size of str
  printf("\n Test end of string at colon \n");
  char *test_string2 = "host:";
  
  
  field_status = is_field("host", test_string2, 
			  test_string2 + 4, &content_start);
 
  printf("\nExpected: -1\n" ); 
  printf("Result: %d\n", field_status);

}

 
void test_get_field_content(void){
  char *test_string = "This_is_test_content\r\n";
  char result[100];

  int result_size = 
    get_field_content(test_string, test_string +21,
		      result,	sizeof(result));

  printf("\nExpected: \"%s\"", test_string); 
  printf("Result: \"%s\"\n", result);
  if(21 == result_size){
    printf("SUCCESS: same length\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      21, result_size);


  //test remove spaces at beginning
  printf("\ntest remove spaces at beginning\n");
  test_string = "   This_is_test_content\r\n";
  result_size =  get_field_content(test_string,
				   test_string +24,
				   result,
				   sizeof(result));
  printf("\nExpected: \"%s\"", test_string); 
  printf("Result: \"%s\"\n", result);
  if(21  == result_size){
    printf("SUCCESS: same length\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      21 , result_size);


  // // test the continuation onto a new line
  printf("\ntest Continuation onto new line\n");
  test_string = "This_is_\r\n test_content\r\n";
  result_size =  get_field_content(test_string, 
				   test_string +24,
				   result,
				   sizeof(result));
  printf("\nExpected: \"This_is_test_content\"\n"); 
  printf("Result: \"%s\"\n", result);
  if(21 == result_size){
    printf("SUCCESS: same length\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      21, result_size);


  //test if space occured in field
  printf("\ntest if space occured in field\n");
  test_string = "This_is_test_conte nt\r\n";
  result_size =  get_field_content(test_string, 
				   test_string +22,
				   result,
				   sizeof(result));
  printf("\nExpected: \"This_is_test_conte\"\n"); 
  printf("Result: \"%s\"\n", result);
  if(19 == result_size){
    printf("SUCCESS: same length\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      19, result_size);


  // Test new line in field
  printf("\nTest new line in field\n");
  test_string = "This_is\n_test_content\r\n";
  result_size =  get_field_content(test_string, 
				   test_string +22,
				   result,
				   sizeof(result));
  if(result_size == -1) printf("SUCCESS caught newlin in string\n");
  else printf("FAIL Expect -1, Got: %d\n",result_size);


  //Test CR in field
  printf("\nTest CR in field\n");
  test_string = "This_is\r_test_content\r\n";
  result_size =  get_field_content(test_string, 
				   test_string +22,
				   result,
				   sizeof(result));
  if(result_size == -1) printf("SUCCESS caught CR in string\n");
  else printf("FAIL Expect -1, Got: %d\n",result_size);


  //Test storage array not big enough
  char result2[20];
  printf("\nTest storage array not big enough\n");
  test_string = "This_is_test_content\r\n";
  result_size =  get_field_content(test_string, 
				   test_string +21,
				   result2,
				   sizeof(result2));
  if(result_size == -1) printf("SUCCESS storage array not big enough\n");
  else printf("FAIL Expect -1, Got: %d\n",result_size);


  //test storage array is just big enough
  printf("\ntest storage array is just big enough\n");
  test_string = "This_is_test_conten\r\n";
  result_size =  get_field_content(test_string, 
				   test_string+20,
				   result2,
				   sizeof(result2));
  printf("\nExpected: \"This_is_test_conten\"\n");
  printf("Result: \"%s\"\n", result2);
  if(20 == result_size){
    printf("SUCCESS: same length\n");
  }
  else printf("FAIL Expected:%ld, Got: %d\n", 
	      sizeof(result2), result_size);


  // Test if did not find end sequence befor filling buffer
  printf("\nTest if did not find end sequence befor filling buffer\n");
  test_string = "This_is_test_contejhfsn\r\n";
  result_size =  get_field_content(test_string, 
				   test_string+26,
				   result2,
				   sizeof(result2));
 
  if(-1 == result_size){
    printf("SUCCESS: Caught the bad header\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      -1, result_size);


  //Test if there is no ending sequence
  printf("\nTest if there is no ending sequence\n");
  test_string = "This_is_test_conteng";
  result_size =  get_field_content(test_string, 
				   test_string+20,
				   result2,
				   sizeof(result2));
 
  if(-1 == result_size){
    printf("SUCCESS: Caught the bad header\n");
  }
  else printf("FAIL Expected:%d, Got: %d\n", 
	      -1, result_size);

} 


void test_find_non_whitespace(void){
 
  printf("\nTest white space string\n");
  char *test_string = "         ";
  char *result = find_non_whitespace(test_string, test_string + 5);
  
  if(result == NULL){
    printf("SUCCESS - Expected: (NULL) 1, Got: %d\n", (result == NULL));
  }
  else printf("FAIL - Expected: NULL, Got: %c\n", *result);

  
  printf("\nTest string which contain characters\n");
  test_string = "   hello      ";
  result = find_non_whitespace(test_string, test_string + 8);
  if( result == test_string + 3 ){
    printf("SUCCESS - Expected: h, Got: %c\n", *result);
  }
  else  printf("FAIL - Expected: h, Got: %c\n", *result);


  printf("\nNo White space\n");
  test_string = "hello";
  result = find_non_whitespace(test_string, test_string + 4);
  if( result == test_string ){
    printf("SUCCESS - Expected: h, Got: %c\n", *result);
  }
  else  printf("FAIL - Expected: h, Got: %c\n", *result);
  
}


void test_tag_header(void){
  char read_buffer[8012];
  int read_status;
  struct http_header_info http_header;
  // Testing HTTP_request_short.txt
  printf("\n\n **** Test HTTP_request_short.txt ***\n");
  int file_desc = open("./test_files/HTTP_Request_short.txt", O_RDONLY);
  int return_val;

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
    return;
  }
 
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  parse_header(&http_header, read_buffer, read_status);
  return_val = tag_header(&http_header);

  printf("\nTest return fields\n"); 
  if(return_val == read_status){
    printf("SUCCESS return value correct\n");
  }
  else printf("FAILED TEST: return value of %d\n", return_val);

  printf("Testing the number of fields\n");
  if(http_header.num_fields == 2){
    printf("SUCCESS, returned (2)\n");
  }
  else printf("FAILED expected 2 got: %d\n", http_header.num_fields);


  printf("\n Printing fields tagged\n");
  printf("1st Field: ");
  print_header_field(&http_header, 0);
  printf("\n2nd Field: ");
  print_header_field(&http_header, 1);

  close(file_desc);


  // HTTP_request.txt test
  printf("\n\n *** Test HTTP_request.txt *** \n");
  file_desc = open("./test_files/HTTP_Request.txt", O_RDONLY);
  
  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  }
 
  parse_header(&http_header, read_buffer, read_status);
  return_val = tag_header(&http_header);

  printf("\nTest return fields\n");
  if(return_val == read_status){
    printf("SUCCESS return value correct\n");
  }
  else printf("FAILED TEST: return value of %d\n", return_val);

  close(file_desc);


  // HTTP_request_unix_ending.txt test
  printf("\n *** Test HTTP_Request_unix_ending.txt (malformed file");
  file_desc = open("./test_files/HTTP_Request_unix_ending.txt", O_RDONLY);

  read_status = read(file_desc, read_buffer, sizeof(read_buffer));    
  if(read_status == -1){
    printf("Error Reading file: %s", strerror(errno));
  } 
  parse_header(&http_header, read_buffer, read_status);
  return_val = tag_header(&http_header);

  printf("\nTest return fields\n"); 
  if(return_val == BAD_REQUEST){
    printf("SUCCESS return value correct\n");
  }
  else printf("FAILED TEST\n");

  printf("Testing the number of fields\n");
  if(http_header.num_fields == 0){
    printf("SUCCESS, returned (0)\n");
  }
  else printf("FAILED expected 0 got: %d\n", http_header.num_fields);

  close(file_desc);

}


void test_is_CRLF(void){
  char test_string[] = "Hello\r\nWorld";
  char *pos = test_string +5;
  char *end_pos = test_string + 11;

  printf("\n\n Test if pos points to CRLF ");
  printf("\n Expect TRUE(1):\t %d", is_CRLF(pos, end_pos));

  pos  = test_string;
  end_pos = test_string + 11;
  printf("\n\n Test if pos does not point to CRLF");
  printf("\n Expect FALSE(0):\t %d", is_CRLF(pos, end_pos));

  pos  = test_string +2;
  end_pos = test_string;
  printf("\n\n Test if pos does not point to CRLF");
  printf("\n Expect ERROR(-1):\t %d\n", is_CRLF(pos, end_pos));
}

// Test function for find_field_end
void test_find_field_end(void){
  char test_string[] = "Hello World \r\nOther side";
  char *start, *end;
  
  start = test_string;
  end = test_string + 18;
  printf("\nstart character(H)-%c \nEnd character(r)-%c\n", *start, *end);
  printf("position of \\r\\n (13):\t %ld\n", 
	 find_field_end(start, end) - start); 
 

  // Different starting point
  start = test_string+ 4;
  end = test_string + 18;
  printf("\nstart character(o)-%c \nEnd character(r)-%c\n", *start, *end);
  printf("position of \\r\\n (9):\t %ld\n", 
	 find_field_end(start, end) - start); 
  

  // No end of line value occured
  start = test_string;
  end = test_string + 5;
  printf("\nNo End of line value occured\n");
  printf("Exect NULL(1):\t %d\n", NULL == find_field_end(start, end) ); 


  // start and end are the same
  start = test_string; 
  end = test_string; 
  printf("\nwhen start and end are the same - Expect NULL\n");
  printf("Expect (1):\t %d\n",NULL == find_field_end(start, end)); 


  /* end is lower than start
     end = test_string;
     start = start + 18;
     printf("\nWhen end is lower than start - Expect Error\n");
     printf("%c", find_field_end(start, end)); 
  */
}



void test_find_non_CRLF(void){
  char test_string[] = "\r\n\r\n\r\n\r\n\r\n\r\nHello \r\n\r\n";
  char *start, *end;
  
  start = test_string;
  end = test_string + 19;
  printf("\nNormal test\n");
  printf("Expect Character of first non CRLF(H): %c \n", 
	 *(find_non_CRLF(start, end))); 

 
  // Different starting point
  start = test_string+ 4;
  end = test_string + 19;
  printf("\nDifferent starting point\n");
  printf("Expect Character of first non CRLF(H):%c\n",
	 *find_non_CRLF(start, end)); 
  

  // No end of line value occured
  start = test_string;
  end = test_string + 5;
  printf("\nNo non CRLF characters occured\n");
  printf("Expect NULL(1) -> %d\n", NULL == find_non_CRLF(start, end)); 


  // start and end are the same
  start = test_string; 
  end = test_string; 
  printf("\nwhen start and end are the same - Expect NULL\n");
  printf("Expect (1) -> %d\n",NULL == find_non_CRLF(start, end)); 

}



