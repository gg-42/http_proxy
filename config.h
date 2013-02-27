/*
 *
 * config.h - function prototypes for the config handling routines
 *
 * by RHE, Oct, 2005.
 */

#include <stdlib.h>
#include <stdio.h>

#ifndef CONFIG_H
#define CONFIG_H

#define CONF_PATH_LEN 256

struct config_token {
	char *token;
	char *value;
	struct config_token *next;
};

struct config_sect {
	char *name;
	struct config_token *tokens;
	struct config_sect *next;
};

struct config_sect *config_load (char *filename);
char *config_get_value (struct config_sect *sect, char *section, char *token, int icase);
void config_dump (struct config_sect *sects);
void config_destroy (struct config_sect *sects);

char * parseArgs(int argc, char * argv[], int *rate_limiting);
char * extractListPort(struct config_sect *config_options);
int extractDebugLevel(struct config_sect *config_options);
#endif
