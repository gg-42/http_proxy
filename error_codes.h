/******************************** error_codes.h **************************
 Description: Defines negative HTTP error codes. This is done so the error codes
  can be used to indicate errors in the program

  SOURCE : http://en.wikipedia.org/wiki/List_of_HTTP_status_codes

 Author(s): Sebastian Carroll (u4395897), Geoffrey Goldstraw (u4528129)

 Version:          $Rev: 49 $
 Date Modified:    $LastChangedDate: 2012-05-25 12:09:34 +1000 (Fri, 25 May 2012) $
 Last Modified by: $Author: u4528129 $

***************************************************************************/

#define BAD_REQUEST        -400 
#define UNAUTHORIZED       -401
#define PAYMENT_REQ        -402
#define FORBIDDEN          -403
#define NOT_FOUND          -404
#define METHOD_NOT_ALLOWED -405
#define NOT_ACCEPTABLE     -406
#define PROXY_AUTH_REQ     -407
#define REQUEST_TIMEOUT    -408
#define CONFLICT           -409
#define GONE               -410
#define LENGTH_REQUIRED    -411
#define PRECOND_FAILED     -412
#define REQUEST_ENT_TOO_LARGE   -413
#define REQUEST_URI_TOO_LONG    -414
#define UNSUPPORT_MEDIA_TYPE    -415
#define REQUEST_RANGE_NOT_SATIS -416
#define EXPECTATION_FAILED -417
#define IM_A_TEAPOT        -418

