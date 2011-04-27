#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"

#define   MAX_MAPS_PER_POOL	25
#define   MAX_POOLS		MAX_RESPONSE_MSGES - 2

static int npools = 0;
static struct pool **pools = NULL;
static const char command[] = "PRIVMSG";

/*
 * Represents a map pool.
 *
 * name is the name of the pool (e.g. the tournament or league
 * of the pool's origin).
 *
 * maps is an array of maps that exist in the pool.
 *
 * nmaps is the number of maps in the pool.
 */
struct pool
{
	char *name;
	char **maps;
	int nmaps;
};

/*
 * Creates and initializes a pool struct
 *
 * name is the name of the map pool. The pointer must point to a
 * memory location that has already been allocated.
 *
 * maps is an array of maps contained in this pool. Each map
 * must already have been allocated.
 *
 * nmaps is the number of maps in the pool.
 */
struct pool *create_pool(char *name, char **maps, int nmaps)
{
	struct pool *p;
	int i;

	p = malloc(sizeof(struct pool));
	p->name = name;
	p->nmaps = nmaps;

	p->maps = malloc(sizeof(char *) * nmaps);
	for (i = 0; i < nmaps; i++)
		p->maps[i] = maps[i];

	return p;
}

/*
 * Frees a map pool and all associated memory.
 */
void free_pool(struct pool *p)
{
	int i;

	for (i = 0; i < p->nmaps; i++)
		free(p->maps[i]);

	free(p->name);
	free(p);

	return;
}

static int filter(const struct dirent *d)
{
	if (strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0)
		return 0;

	return 1;
}

char *get_command()
{
	return (char *)command;
}

int initialize()
{
	FILE *fp;
	char buf[100];
	char *maps[MAX_MAPS_PER_POOL];
	struct dirent **namelist;
	int n, i;

	npools = 0;
	pools = malloc(sizeof(struct pool *) * MAX_POOLS);
	n = scandir("../rmap", &namelist, &filter, alphasort);

	if (n < 0) {
		perror("scandir");
		exit(EXIT_FAILURE);
	}

	while (n-- && npools < MAX_POOLS) {
		char location[100] = "../rmap/";
		strcpy(location + 8, namelist[n]->d_name);

		fp = fopen(location, "r");
		if (!fp)
			return 0;

		i = 0;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			char *tok;

			if (strrchr(buf, '\n') == NULL)
				continue;

			tok = strtok(buf, "\r\n");
			if (tok == NULL)
				continue;
			maps[i]= strdup(tok);
			i++;

		}
		fclose(fp);

		/* only count this as a valid map pool if the pool has
		 * at least one map */
		if (i > 0) {
			struct pool *p;
			char *name;

			name = strdup(namelist[n]->d_name);
			p = create_pool(name, maps, i);
			pools[npools] = p;
			npools++;
		}

		free(namelist[n]);
	}

	if (npools == MAX_POOLS) {
		printf("Attemped to load more than the max allowable number of"
				"map pools (%d).", MAX_POOLS);

	}
	
	free(namelist);
	return 0;
}

int close()
{
	int i;

	for (i = 0; i < npools; i++)
		free_pool(pools[i]);
	free(pools);

	return 0;
}
int create_response(struct irc_message *msg,
		struct irc_message **messages, int *msg_count)
{
	char buf[IRC_BUF_LENGTH];
	char *msg_message, *command, *n, *c;

	/* channel */
	c = strtok(msg->params, " ");

	/* message */
	msg_message = strtok(NULL, "") + 1;

	/* command (e.g. !pools) */
	command = strtok(msg_message, " ");
	if (command == NULL)
		return 0;

	/* pool number (if specified) */
	n = strtok(NULL, " ");

	if (strcmp(command, "!pools") == 0) {
		int i;

		sprintf(buf, "%s :%s",
				c,
				"Available map pools:");
		messages[*msg_count] =
			create_message(NULL, "PRIVMSG", buf);
		if (messages[*msg_count])
			(*msg_count)++;

		sprintf(buf, "%s :%s", 
				c, 
				"All pools (don't specify a number)");
		messages[*msg_count] = 
			create_message(NULL, "PRIVMSG", buf);
		if (messages[*msg_count])
			(*msg_count)++;

		for (i = 0; i < npools; i++) {
			sprintf(buf, "%s :%d. %s", 
					c, i+1, pools[i]->name);
			messages[*msg_count] = 
				create_message(NULL, "PRIVMSG", buf);
			if (messages[*msg_count])
				(*msg_count)++;
		}
	} else if (strcmp(command, "!reload") == 0) {
		close();
		initialize();

		sprintf(buf, "%s :%s",
				c,
				"Reloaded map pools");
		messages[*msg_count] =
			create_message(NULL, "PRIVMSG", buf);
		if (messages[*msg_count])
			(*msg_count)++;
	} else if (strcmp(command, "!rm") == 0) {
		int pn, choice;

		if (n != NULL) {
			/* pick a map from the specified pool */
			pn = atoi(n);	
			if (pn == 0 || pn > npools)
				return -1;
			pn--;
			
			choice = random() % (pools[pn]->nmaps);

			sprintf(buf, "%s :Map: %s",
					c, pools[pn]->maps[choice]);
			messages[*msg_count] =
				create_message(NULL, "PRIVMSG", buf);
			if (messages[*msg_count])
				(*msg_count)++;
		} else {
			/* pick a map from the union of all pools */
			int totalmaps, i;

			for (i = 0; i < npools; i++)
				totalmaps += pools[i]->nmaps;
			choice = random() % totalmaps;

			i = 0;
			while (choice >= pools[i]->nmaps) {
				choice -= pools[i]->nmaps;
				i++;
			}

			sprintf(buf, "%s :Map: %s",
					c, pools[i]->maps[choice]);
			messages[*msg_count] =
				create_message(NULL, "PRIVMSG", buf);
			if (messages[*msg_count])
				(*msg_count)++;
		}
	}
	
	return 0;
}
