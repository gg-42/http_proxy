/******************************** tests.c **********************************
 Description: 
   Contains main method to call all testing functions in various source 
   files.
             
 Author(s): Geoffrey Goldstraw (u4528129)        

 Version:          $Rev: 41 $
 Date Modified:    $LastChangedDate: 2012-05-24 19:54:54 +1000 (Thu, 24 May 2012) $
 Last Modified by: $Author: u4528129 $ 

***************************************************************************/

#include<stdio.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "header_parser.h"


void test1_read(void);


// Test Suit
int main(void){
  //  test1_read();
  header_parser_tests();
  return 0;
}



void test1_read(void){
  printf("\nTest 1 -  \n");
  
  int file_desc = open("./test_files/HTTP_Request.txt", O_RDONLY);

  if(file_desc == -1){
    printf("Error Occured opening file: %s", strerror(errno));
  }

  char buf[100 +1];
  int  read_status;

  while(1){
    
    read_status = read(file_desc, buf, sizeof(buf)-1);    
 
    if(read_status == -1){
      printf("Error Reading file: %s", strerror(errno));
      return;
    }
    else if(read_status > 0){
      buf[read_status] = '\0';
      printf("%s", buf); 
    }
    else return;
  }

  close(file_desc);  
}


