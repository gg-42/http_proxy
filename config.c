/**************************************************************************
 * config.c - a sectional configuration file parser
 *
 * by RHE, Oct, 2005.
 *
 * This parser is intended to parse configuration files with sections
 * delimited by section headers of the form of [section]. Multiple sections
 * of each section type can be parsed.
 * Within each section, configuration values are of the form:
 *   token = value
 * or
 *   token value
 *
 */

#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include "config.h"

#define MAX_CONF_LEN 256
#include <string.h>

// Parses the command line args
char * parseArgs(int argc, char * argv[], int *rate_limiting){
    int i;
    char * fileName = NULL;
    for(i = 0; i < argc; i++){
        if(strcmp(argv[i], "-f")==0 && argv[i+1] != NULL){
//        	return argv[i+1];
            fileName = argv[i+1];
        }
        if(strcmp(argv[i], "-rl")==0){
            *rate_limiting = 1;
        }
    }
//    return NULL;
    return fileName;
}

// Searches through the options read from the conf file to find
// the port specified therein
char * extractListPort(struct config_sect *config_options){
    char * section = "default";
    char * tokenToFind = "proxy_port";
    while (config_options) {
        if (strcmp (section, config_options->name) == 0) {
            struct config_token *tokens = config_options->tokens;
            while (tokens) {
                if(strcmp(tokenToFind, tokens->token) == 0){
                    return tokens->value;
                }
                tokens = tokens->next;
            }
        }
        config_options = config_options->next;
    }
    return NULL;
} // End extractListPort

// Sets the global int 'debug_mode' to the appropriate value set
// in the config file
int extractDebugLevel(struct config_sect *config_options){
    char * section = "default";
    char * tokenToFind = "debug";
    while (config_options) {
        if (strcmp (section, config_options->name) == 0) {
            struct config_token *tokens = config_options->tokens;
            while (tokens) {
                if(strcmp(tokenToFind, tokens->token) == 0){
                    return atoi(tokens->value);
                }
                tokens = tokens->next;
            }
        }
        config_options = config_options->next;
    }
    return -1;
} // End extractDebugLevel

/*
 * allocate a new [section] structure
 */
struct config_sect *config_new_section (char *name)
{
	struct config_sect *new_sect;

	new_sect = (struct config_sect *) malloc (sizeof (struct config_sect));
	if (new_sect) {
		new_sect->name = strdup (name);
		new_sect->tokens = NULL;
		new_sect->next = NULL;
	}
	return (new_sect);
} /* config_new_section () */

struct config_token *config_new_token (char *token, char *value)
{
	struct config_token *new_token;

	new_token = (struct config_token *) malloc (sizeof (struct config_token));
	if (new_token) {
		new_token->token = strdup (token);
		new_token->value = strdup (value);
		new_token->next = NULL;
	}
	return (new_token);
} /* config_new_token () */

/*
 * load a configuration file into memory, allocating structures as required
 */
struct config_sect *config_load (char *filename)
{
	FILE *fp;
	char line[MAX_CONF_LEN];
	struct config_sect *sects = NULL;
	struct config_sect *cur_sect = NULL;
	struct config_token *cur_token = NULL;
	char *token;
	char *value;

	fp = fopen (filename, "r");
	if (!fp) {	
		perror ("config file");
		return NULL;
	}

	while (fgets(line, MAX_CONF_LEN, fp)) {
		if (line[0] == '#') continue;	/* skip lines with comment */
		if (strlen (line) <= 1) continue;
		line[strlen (line) - 1] = 0;	/* remove trailing '\n' */
		if (line[0] == '[') {		/* section header */
			token = strtok (line + 1, "]");
			if (token != NULL) {
				struct config_sect *new_sect = config_new_section (token);
				if (new_sect) {
					if (cur_sect) {
						cur_sect->next = new_sect;
					} else {
						sects = new_sect;
					}
					cur_sect = new_sect;
					cur_token = NULL;
				}
			}
		} else {
			token = strtok_r (line, " \t=", &value);
			if (token != NULL) {
				struct config_token *new_token;
				value = strtok (value, " =#\t");
				if (!cur_sect) {
					cur_sect = config_new_section ("default");
					sects = cur_sect;
				}
				new_token = config_new_token (token, value);
				if (new_token) {
					if (cur_token) {
						cur_token->next = new_token;
					} else {
						cur_sect->tokens = new_token;
					}
					cur_token = new_token;
				}
			}
		}
	}
	fclose(fp);
	return sects;
} /* config_load () */

char *config_get_value (struct config_sect *sects, char *section, char *token, int icase)
{
	while (sects) {
		if ((icase ? strcasecmp (section, sects->name) : strcmp (section, sects->name)) == 0) {
			struct config_token *tokens = sects->tokens;
			while (tokens) {
				if ((icase ? strcasecmp (token, tokens->token) : strcmp (token, tokens->token)) == 0) {
					return (tokens->value);
				}
				tokens = tokens->next;
			}
		}
		sects = sects->next;
	}
	return (NULL);
} /* config_get_value () */

void config_dump (struct config_sect *sects)
{
	while (sects) {
		printf ("[%s]\n", sects->name);
		struct config_token *tokens = sects->tokens;
		while (tokens) {
			printf ("%s\t%s\n", tokens->token, tokens->value);
			tokens = tokens->next;
		}
		sects = sects->next;
	}
} /* config_dump () */

void config_destroy (struct config_sect *sects)
{
	while (sects) {
		struct config_sect *next = sects->next;
		struct config_token *token = sects->tokens;
		while (token) {
			struct config_token *next_token = token->next;
			free (memset (token->token, 0, strlen (token->token)));
			free (memset (token->value, 0, strlen (token->value)));
			free (memset (token, 0, sizeof (struct config_token)));
			token = next_token;
		}
		free (memset (sects->name, 0, strlen (sects->name)));
		free (memset (sects, 0, sizeof (struct config_sect)));
		sects = next;
	}
} /* config_destroy () */
