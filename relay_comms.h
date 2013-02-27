/******************************** relay_comms.h ********************************
 Description:
  Contains functions to rate limit and relay HTTP messages between client and
  server.

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 50 $
 Date Modified:    $LastChangedDate: 2012-05-25 14:13:43 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4395897 $

*******************************************************************************/

#include "rate_lib.h"


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
int relay(int client_socket,   struct config_sect * config_options, int rate_limiting);


/* Sets up a listening connection if Host == NULL -> suitable for a server
 *  else sets up a direct connection suitable for a client
 *
 *  Returns: Socket if successful
 *			-1 if an error in creating the socket has occurred
 */
int setup_socket(char * port, char * host);

// Testing functions
void relay_tests(void);
