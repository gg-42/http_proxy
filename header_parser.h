/******************************** header_parser.h **************************
 Description: 
  This contains functions to read and parse a http header message.
             
 Author(s): Geoffrey Goldstraw (u4528129)        

 Version:          $Rev: 41 $
 Date Modified:    $LastChangedDate: 2012-05-24 19:54:54 +1000 (Thu, 24 May 2012) $
 Last Modified by: $Author: u4528129 $ 

***************************************************************************/


//If this is changed it needs to be a multiple of MAX_HEADER_LENGTH
#include "defaults.h"

struct http_header_info {
  char *read_storage;
  char *header_fields[MAX_NUM_FIELDS];
  char *header_end, *end_data ;
  int num_fields;
}; 



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
					  int sizeof_RX_data);



/*** The following procedures must read in the header field using parse_header()
      before they can be used. ***/
 

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
				 int sizeof_url_storage);


/* Finds gets the content-length field
   Return  Length of the body
     -1 if error has occurred*/
int get_content_length(struct http_header_info *http_header);


/* Prints the header information */
void print_header(struct http_header_info *http_header);

/* Prints a particular header field
 * field_num is index from 0 */
int print_header_field(struct http_header_info *http_header, int field_num);

// Print end - start bytes starting at start
void print_range(char *start, char *end);


// TEST FUNCTION HEADERS
void header_parser_tests(void);

