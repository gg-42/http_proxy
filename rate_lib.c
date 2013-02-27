/******************************** rate_lib.c **************************
 Description:
  Rate and timing functions

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 56 $
 Date Modified:    $LastChangedDate: 2012-05-25 15:18:58 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4395897 $

***************************************************************************/


#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>

#include "rate_lib.h"
#include "defaults.h"

/****************************************************************************/
int time_compare(struct timeval *time1, struct timeval *time2);

// TESTING FUNCTIONS
void test_suspend(void);
/*****************************************************************************/


/* Searches through the config options to determine the size of the outgoing
 * buffer used for rate limiting.
 */
int get_rate_limit(struct config_sect * config_options, char * host_address){
  char *section = "rates";

  while (config_options) {
    if (strcmp (section, config_options->name) == 0) {
      //             printf("Looking at rates to limit domain\n");
      struct config_token *tokens = config_options->tokens;
      while (tokens) {
	if(strstr( host_address, tokens->token) != NULL){
	  //                     printf("Matched entry: %s\n", tokens->token);
	  return atoi(tokens->value);
	}
	tokens = tokens->next;
      }
    }
    config_options = config_options->next;
  }
  //     printf("Domain not matched: %s\n", host_address);
  return RATE_LIMIT;
}// End get_rate_limit


/* Simple function to convert form kB/s to B/ms
 * if rate = x kB/s then rate = x*1024/1000 B/ms = x*1.024 B/ms */
int convertToBpInterval(int rate_limit){
  float k = 1024;
  return (int) rate_limit*k;
}



/* Suspends process if bin amount is empty and deadline has not been reached.
 * If the deadline has been passed then the bin_amount is reset to its maximum
 * value
 *
 * Return -1  error occurred in getting time
 *         1 successful completion
 */
int suspend(struct rate *rate_limit){
  assert(rate_limit != NULL);

  struct timeval current_time, deadline, sleeptime;
  // find the deadline
  timeradd(&(rate_limit -> timestamp), &(rate_limit -> period), &deadline);

  if(gettimeofday(&current_time, NULL) < 0){
    printf("getting clock information failed: %s", strerror(errno));
    return -1;
  }

  // if have not reached deadline and bin_amount is 0 then reset bin_amount and
  // suspend until reach timer
  if(time_compare(&current_time, &deadline) <= 0  && rate_limit -> bin_amount <= 0){
    timersub( &deadline, &current_time , &sleeptime);
    printf("Going to sleep");
    select(0,0,0,0,&sleeptime); // sleep until sleeptime has expired
    rate_limit -> timestamp = deadline;
    rate_limit -> bin_amount = rate_limit ->bin_max_amount;
  }
  else if(time_compare(&current_time, &deadline) > 0){
    rate_limit -> timestamp = current_time;
    rate_limit -> bin_amount = rate_limit -> bin_max_amount;
  }
  return 1;
} // End suspend



/*Update the bin given that an amount was sent */
void update_bin(int amount_sent, struct rate *rate_limit){
  assert(amount_sent >= 0);
  assert(rate_limit != NULL);

  rate_limit -> bin_amount = rate_limit -> bin_amount - amount_sent;
} // End update_bin



/*Compares two timeval structs
 * Returns:
 *      -1  time1 < time2
 *       0  time1 == time2
 *       1  time1 > time2
 */
int time_compare(struct timeval *time1, struct timeval *time2){
  // pre condition
  assert((time1 -> tv_sec) >= 0);
  assert((time1 -> tv_usec) >= 0);
  assert((time2 -> tv_sec) >= 0);
  assert((time2 -> tv_usec) >= 0);

  if( (time1 -> tv_sec) > (time2 -> tv_sec) ) return 1;
  else if ((time1 -> tv_sec) < (time2 -> tv_sec) ) return -1;
  else{
    if( (time1 -> tv_usec) > (time2 -> tv_usec) ) return 1;
    else if ((time1 -> tv_sec) < (time2 -> tv_sec) ) return -1;
    else return 0;
  }
} // End time_compare



void print_timeval(struct timeval *value){
  printf("%ld sec %ld usec", value -> tv_sec, value -> tv_usec); 
}


/******************************TEST FUNCTIONS **************************/
void rate_lib_tests(void){
  test_suspend();

}


void test_suspend(void){
  int amount, max_amount;
  max_amount = 3;
  amount = 0;

  const struct timeval reader_timeout = {.tv_sec = 3, .tv_usec = 0};
  struct timeval  exp_time;
  struct rate rate_limit = {.period = reader_timeout, .bin_amount = amount,
      .bin_max_amount = max_amount };

  gettimeofday(&(rate_limit.timestamp), NULL);
  timeradd(&(rate_limit.timestamp), &reader_timeout, &exp_time);
  suspend(&rate_limit);
  printf("\nExpect:"); print_timeval(&exp_time);
  printf("\nGot: ");  print_timeval(&(rate_limit.timestamp));
  printf("\nAmount(3):%d\n", amount);
  


  gettimeofday(&(rate_limit.timestamp), NULL);
  exp_time = rate_limit.timestamp;
  rate_limit.bin_amount = 2;
  suspend(&rate_limit);
  printf("\n\nExpect:"); print_timeval(&exp_time);
  printf("\nGot: ");  print_timeval(&(rate_limit.timestamp));
  printf("\nAmount(2):%d\n", amount);


  gettimeofday(&rate_limit.timestamp, NULL);
  amount = 2;

  select(0,0,0,0,&((struct timeval) {.tv_sec = 4, .tv_usec = 0}));
  gettimeofday(&exp_time, NULL); 
  suspend(&rate_limit);
  printf("\n\nExpect:"); print_timeval(&exp_time);
  printf("\nGot: ");  print_timeval(&(rate_limit.timestamp));
  printf("\nAmount(3):%d\n", amount);
  
}


