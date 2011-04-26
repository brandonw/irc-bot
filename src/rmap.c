/*#include <errno.h>*/
/*#include <glib.h>*/
/*#include <limits.h>*/
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"

#define   MAX_MAPS_PER_POOL	25

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

int create_response(struct irc_message *msg,
		struct irc_message **messages, int *msg_count)
{
	char buf[IRC_BUF_LENGTH];
	char *msg_message, *tok, *n;

	strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;
	tok = strtok(msg_message, " ");

	n = strtok(NULL, " ");
	if (n == NULL)
		return 0;

	/*if (strcmp(tok, "!pools") == 0) {*/
		/*int i;*/

		/*for (i = 0; i < npools; i++) {*/
			/*[> print pools <]*/

		/*}*/
		/*karma = (long *)g_hash_table_lookup(karma_hash, n);*/
		/*if (karma != NULL) {*/
			/*k = *karma;*/
		/*}*/

		/*sprintf(buf, "%s :%s has %ld karma", channel, n, k);*/
		/*messages[0] = create_message(NULL, "PRIVMSG", buf);*/
		/*if (messages[0])*/
			/**msg_count = 1;*/
	/*} [>else if (strcmp(tok, "!up") == 0) {<]*/
		/*karma = (long *)g_hash_table_lookup(karma_hash, n);*/
		/*if (karma == NULL) {*/
			/*karma = malloc(sizeof(long));*/
			/**karma = 0;*/
			/*nick = strdup(n);*/
			/*g_hash_table_insert(karma_hash, nick, karma);*/
		/*}*/
		/*(*karma)++;*/

		/*sprintf(buf, "%s :%s has been upvoted to %ld karma",*/
			/*channel, n, *karma);*/
		/*messages[0] = create_message(NULL, "PRIVMSG", buf);*/
		/*if (messages[0])*/
			/**msg_count = 1;*/
	/*} else if (strcmp(tok, "!down") == 0) {*/
		/*karma = (long *)g_hash_table_lookup(karma_hash, n);*/
		/*if (karma == NULL) {*/
			/*karma = malloc(sizeof(long));*/
			/**karma = 0;*/
			/*nick = strdup(n);*/
			/*g_hash_table_insert(karma_hash, n, karma);*/
		/*}*/
		/*(*karma)--;*/

		/*sprintf(buf, "%s :%s has been downvoted to %ld karma",*/
			/*channel, n, *karma);*/
		/*messages[0] = create_message(NULL, "PRIVMSG", buf);*/
		/*if (messages[0])*/
			/**msg_count = 1;*/
	/*}*/

	return 0;
}

int initialize()
{
	FILE *fp;
	char buf[100];
	char *maps[MAX_MAPS_PER_POOL];
	struct dirent **namelist;
	int n, i;

	pools = malloc(sizeof(struct pool *) * MAX_RESPONSE_MSGES);
	n = scandir("../rmap", &namelist, &filter, alphasort);

	if (n < 0) {
		perror("scandir");
		exit(EXIT_FAILURE);
	}

	while (n-- && npools < MAX_RESPONSE_MSGES) {
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

	if (npools == MAX_RESPONSE_MSGES) {
		printf("Attemped to load more than the max allowable number of"
				"map pools (%d).", MAX_PLUGINS);

	}
	
	free(namelist);

	for (i = 0; i < npools; i++) {
		printf("%s ------------------\n", pools[i]->name);
		for (n = 0; n < pools[i]->nmaps; n++) {
			printf("%s\n", pools[i]->maps[n]);
		}
	}

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
