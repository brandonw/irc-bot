#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "string.h"
#include "errno.h"
#include "debug.h"

#define   RM_MAX_MAPS_PER_POOL	25
#define   RM_MAX_POOLS		25

static char RM_POOLS_CMD[] = "pools";
static char RM_POOLS_HELP[] = "displays available map pools";

static char RM_RANDOM_MAP_CMD[] = "rm";
static char RM_RANDOM_MAP_HELP[] = "chooses a random map";

static char RM_RELOAD_MAPS_CMD[] = "reload-maps";
static char RM_RELOAD_MAPS_HELP[] = "reloads map pools from disk";

static char PLUG_NAME[] = "random map pools";
static char PLUG_DESCR[] = "chooses a random map from a pool";

static int npools = 0;
static struct pool **pools = NULL;
static char **commands;
static char **commands_help;
static const int command_qty = 3;

char **get_commands()
{
	return commands;
}

int get_command_qty()
{
	return command_qty;
}

char *get_plug_name()
{
	return PLUG_NAME;
}

char *get_plug_descr()
{
	return PLUG_DESCR;
}

char **get_plug_help()
{
	return commands_help;
}

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

	p = malloc(sizeof(*p));
	p->name = name;
	p->nmaps = nmaps;

	p->maps = malloc(sizeof(*(p->maps)) * nmaps);
	for (i = 0; i < nmaps; i++)
		p->maps[i] = maps[i];

	log_info("Created pool %s", name);
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
	free(p->maps);
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

int plug_init()
{
	FILE *fp;
	char buf[100];
	char *maps[RM_MAX_MAPS_PER_POOL];
	struct dirent **namelist;
	int n, i;

	commands = malloc(sizeof(*commands) * command_qty);
	commands[0] = RM_POOLS_CMD;
	commands[1] = RM_RANDOM_MAP_CMD;
	commands[2] = RM_RELOAD_MAPS_CMD;

	commands_help = malloc(sizeof(commands_help) * command_qty);
	commands_help[0] = RM_POOLS_HELP;
	commands_help[1] = RM_RANDOM_MAP_HELP;
	commands_help[2] = RM_RELOAD_MAPS_HELP;

	npools = 0;
	pools = malloc(sizeof(*pools) * RM_MAX_POOLS);
	n = scandir("../rmap", &namelist, &filter, alphasort);

	if (n < 0) {
		log_err("Error scanning directory for plugins: %s",
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	while (n-- && npools < RM_MAX_POOLS) {
		char location[100] = "../rmap/";
		strcpy(location + 8, namelist[n]->d_name);

		fp = fopen(location, "r");
		if (!fp) {
			log_warn("%s: Error loading  maps: %s", location,
					strerror(errno));
			continue;
		}

		i = 0;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			char *tok;

			if (strrchr(buf, '\n') == NULL) {
				log_warn("%s:%d was too long to load.",
						location, i + 1);
				continue;
			}

			tok = strtok(buf, "\r\n");
			if (tok == NULL) {
				log_warn("%s:%d blank line ignored.",
						location, i + 1);
				continue;
			}
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
		else {
			log_warn("%s had no maps listed.", location);
		}

		free(namelist[n]);
	}

	if (npools == RM_MAX_POOLS) {
		log_warn("Too many map pools; truncating remaining pools.");
	}

	free(namelist);
	return 0;
}

int plug_close()
{
	int i;

	free(commands);
	free(commands_help);

	for (i = 0; i < npools; i++)
		free_pool(pools[i]);
	free(pools);

	return 0;
}

int cmd_reply(char *src, char *dest, char *cmd, char *msg)
{
	char buf[IRC_BUF_LENGTH];
	struct plug_msg *tmp;
	char *n;

	/* pool number (if specified) */
	n = strtok(msg, " ");

	if (strcmp(cmd, RM_POOLS_CMD) == 0) {
		int i;

		sprintf(buf, "%s", "Available map pools:");
		tmp = create_plug_msg(src, buf);
		send_plug_msg(tmp);
		free_plug_msg(tmp);

		sprintf(buf, "%s", "All pools (don't specify a number)");
		tmp = create_plug_msg(src, buf);
		send_plug_msg(tmp);
		free_plug_msg(tmp);

		for (i = 0; i < npools; i++) {
			sprintf(buf, "%d. %s", i+1, pools[i]->name);
			tmp = create_plug_msg(src, buf);
			send_plug_msg(tmp);
			free_plug_msg(tmp);
		}
	} else if (strcmp(cmd, RM_RELOAD_MAPS_CMD) == 0) {
		plug_close();
		plug_init();

		sprintf(buf, "%s", "Reloaded map pools");
		tmp = create_plug_msg(src, buf);
		send_plug_msg(tmp);
		free_plug_msg(tmp);
	} else if (strcmp(cmd, RM_RANDOM_MAP_CMD) == 0) {
		int pn, choice;

		if (n != NULL) {
			/* pick a map from the specified pool */
			pn = atoi(n);
			if (pn == 0 || pn > npools)
				return -1;
			pn--;

			choice = random() % (pools[pn]->nmaps);

			sprintf(buf, "Map: %s",pools[pn]->maps[choice]);
			tmp = create_plug_msg(src, buf);
			send_plug_msg(tmp);
			free_plug_msg(tmp);
		} else {
			/* pick a map from the union of all pools */
			int totalmaps = 0, i;

			for (i = 0; i < npools; i++)
				totalmaps += pools[i]->nmaps;
			choice = random() % totalmaps;

			i = 0;
			while (choice >= pools[i]->nmaps) {
				choice -= pools[i]->nmaps;
				i++;
			}

			sprintf(buf, "Map: %s", pools[i]->maps[choice]);
			tmp = create_plug_msg(src, buf);
			send_plug_msg(tmp);
			free_plug_msg(tmp);
		}
	}

	return 0;
}
