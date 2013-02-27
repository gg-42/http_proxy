/******************************** rate_lib.h **************************
 Description:
  Rate and timing functions

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 49 $
 Date Modified:    $LastChangedDate: 2012-05-25 12:09:34 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4528129 $

***************************************************************************/

#include "config.h"

struct rate{
  struct timeval timestamp, period;
  int bin_amount, bin_max_amount;
};


/* Suspends process if bin amount is empty and deadline has not been reached.
 * If the deadline has been passed then the bin_amount is reset to its maximum
 * value
 *
 * Return -1  error occurred in getting time
 *         1 successful completion
 */
int suspend(struct rate *rate_limit);

/*Update the bin given that an amount was sent */
void update_bin(int amount_sent, struct rate *rate_limit);

/* Searches through the config options to determine the size of the outgoing
 * buffer used for rate limiting.
 */
int get_rate_limit(struct config_sect * config_options, char * host_address);

/* Simple function to convert form kB/s to B/ms
 * if rate = x kB/s then rate = x*1024/1000 B/ms = x*1.024 B/ms */
int convertToBpInterval(int rate_limit);



/** TESTING functions **/
void rate_lib_tests(void);

