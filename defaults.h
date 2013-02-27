/******************************** defaults.h ********************************
 Description:
  Contains the default parameters for the rate-limiting HTTP proxy

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 59 $
 Date Modified:    $LastChangedDate: 2012-05-25 16:46:21 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4395897 $

*******************************************************************************/

// Maximum length of the http header
#define MAX_HEADER_LENGTH 8096

// Maximum number of fields that can be in the http header
#define MAX_NUM_FIELDS 255 

//The maximum number of digits in the content length
#define MAX_CONTENT_LENGTH_DIGITS 10 
#define MAX_URL_SIZE 500 // maximum url length

// Wait time before it will cancel the read operation
#define READ_TIMEOUT_SEC 120
#define READ_TIMEOUT_USEC 0

// Default ratelimit
#define RATE_LIMIT 5

#define SERVER_PORT "80" // Default port to send to
#define DEF_LIS_PORT "8080" // Default listening port

#define MAX_QUEUE 20 // Max queue in accepting connections

//The amount read in before being relayed onto sender when no rate limiting applies
#define RELAY_BUF_SIZE 8096









