#include <stdio.h>
#include <string.h>
#include "config.h"

#define MAX_BACKENDS 4
#define CONF_FILE "test.conf"

int main (int argc, char *argv[]) {
	struct config_sect *sects;

	sects = config_load (CONF_FILE);
	if (sects) {
		config_dump (sects);
	}
	printf ("value of config_get_value (sects, \"default\", \"Proxy_Port\", 0) is %s\n", config_get_value (sects, "default", "Proxy_Port", 0));
	printf ("value of config_get_value (sects, \"default\", \"Proxy_Port\", 1) is %s\n", config_get_value (sects, "default", "Proxy_Port", 1));

	config_destroy (sects);
	config_dump (sects);
	sects = NULL;
	config_dump (sects);
	return (0);
} /* main */

