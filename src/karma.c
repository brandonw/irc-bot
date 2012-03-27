#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "debug.h"

#define   KARMA_FILE            "karma.txt"
#define   KARMA_CHECK_CMD       "karma"
#define   KARMA_UP_CMD          "up"
#define   KARMA_DOWN_CMD        "down"
#define   KARMA_MAX_LINE_LENGTH	100
#define   KARMA_MAX_NICK_LENGTH	80

static GHashTable *karma_hash = NULL;
static char **commands;
static const int command_qty = 3;
static char karma_cmd[] = KARMA_CHECK_CMD;
static char up_cmd[] = KARMA_UP_CMD;
static char down_cmd[] = KARMA_DOWN_CMD;
static char plug_name[] = "karma";

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
	return plug_name;
}

int plug_init()
{
	FILE *fp;
	char buf[KARMA_MAX_LINE_LENGTH];
	int errno, lineno;
	char *n;
	long *k;

	commands = malloc(sizeof(*commands) * command_qty);
	commands[0] = karma_cmd;
	commands[1] = up_cmd;
	commands[2] = down_cmd;

	karma_hash = g_hash_table_new(g_str_hash, g_str_equal);
	lineno = 1;

	fp = fopen(KARMA_FILE, "r");
	if (!fp) {
		log_info("Failed to open %s", KARMA_FILE);
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *tok, *endptr;

		if (strrchr(buf, '\n') == NULL) {
			log_warn("%s:%d too long to process.",
					KARMA_FILE, lineno);
			continue;
		}

		tok = strtok(buf, "\t ");
		if (tok == NULL) {
			log_warn("%s:%d invalid.", KARMA_FILE, lineno);
			lineno++;
			continue;
		}
		if (strlen(tok) > KARMA_MAX_NICK_LENGTH) {
			log_warn("%s:%d nick is too long.",
					KARMA_FILE, lineno);
			lineno++;
			continue;
		}
		n = strdup(tok);

		tok = strtok(NULL, "\t ");
		if (tok == NULL)
		{
			log_warn("%s:%d karma unspecified.",
					KARMA_FILE, lineno);
			free(n);
			lineno++;
			continue;
		}
		k = (long *)malloc(sizeof(*k));
		errno = 0;
		*k = strtol(tok, &endptr, 10);

		if ((errno == ERANGE && (*k == LONG_MAX || *k == LONG_MIN))
				|| (errno != 0 && *k == 0)) {
			log_err("%s:%d karma outside of range or invalid.",
					KARMA_FILE, lineno);
			lineno++;
			continue;
		}
		if (endptr == tok) {
			log_err("%s:%d missing karma.", KARMA_FILE, lineno);
			lineno++;
			continue;
		}

		g_hash_table_insert(karma_hash, (gpointer) n, (gpointer) k);
		debug("Inserted %s with %ld karma in to karma hashmap.", n, *k);
		lineno++;
	}
	fclose(fp);
	return 0;
}

int plug_close()
{
	GList *keys;
	FILE *fp;
	char *nick;
	long *karma;

	free(commands);

	keys = g_hash_table_get_keys(karma_hash);
	fp = fopen(KARMA_FILE, "w");
	if (fp == NULL) {
		log_err("Failed to open %s for writing.", KARMA_FILE);
	}

	while (keys != NULL) {
		nick = keys->data;
		karma = g_hash_table_lookup(karma_hash, nick);
		if (fp != NULL) {
			debug("Writing %s with %ld karma to %s",
					nick, *karma, KARMA_FILE);
			fprintf(fp, "%s\t%ld\n", nick, *karma);
		}

		g_hash_table_remove(karma_hash, (gconstpointer) nick);
		free(nick);
		free(karma);
		keys = g_list_next(keys);
	}
	g_list_free(keys);
	g_hash_table_destroy(karma_hash);

	if (fp == NULL)
		return -1;

	fclose(fp);
	return 0;
}

int create_cmd_response(char *src, char *dest,
		char *cmd, char *msg, struct plug_msg **responses,
		int *count)
{
	char buf[IRC_BUF_LENGTH];
	char *msg_nick;
	long *k;

	*count = 0;

	if (strcmp(cmd, KARMA_CHECK_CMD) != 0 &&
			strcmp(cmd, KARMA_UP_CMD) != 0 &&
			strcmp(cmd, KARMA_DOWN_CMD) != 0)
	{
		return 0;
	}

	msg_nick = strtok(msg, " ");
	if (msg_nick == NULL || strlen(msg_nick) > KARMA_MAX_NICK_LENGTH) {
		if (msg_nick == NULL)
			log_warn("Missing nick in karma plugin.");
		else
			log_warn("%s is too long of a nick.", msg_nick);
		return 0;
	}

	*count = 0;

	if (strcmp(cmd, KARMA_CHECK_CMD) == 0) {
		long tmp_k = 0;
		k = (long *)g_hash_table_lookup(karma_hash, msg_nick);
		if (k != NULL) {
			tmp_k = *k;
		}

		debug("Retrieving %ld karma for %s", tmp_k, msg_nick);

		sprintf(buf, "%s has %ld karma", msg_nick, tmp_k);
		responses[0] = create_plug_msg(channel, buf);
		if (responses[0])
			*count = 1;
	} else if (strcmp(cmd, KARMA_UP_CMD) == 0) {
		char *n;

		k = (long *)g_hash_table_lookup(karma_hash, msg_nick);
		if (k == NULL) {
			k = malloc(sizeof(*k));
			*k = 0;
			n = strdup(msg_nick);
			g_hash_table_insert(karma_hash, n, k);
		}
		(*k)++;

		debug("Upping %s karma to %ld", msg_nick, *k);

		sprintf(buf, "%s has been upvoted to %ld karma",
			msg_nick, *k);
		responses[0] = create_plug_msg(channel, buf);
		if (responses[0])
			*count = 1;
	} else if (strcmp(cmd, KARMA_DOWN_CMD) == 0) {
		char *n;

		k = (long *)g_hash_table_lookup(karma_hash, msg_nick);
		if (k == NULL) {
			k = malloc(sizeof(*k));
			*k = 0;
			n = strdup(msg_nick);
			g_hash_table_insert(karma_hash, n, k);
		}
		(*k)--;

		debug("Reducing %s karma to %ld", msg_nick, *k);

		sprintf(buf, "%s has been downvoted to %ld karma",
			msg_nick, *k);
		responses[0] = create_plug_msg(channel, buf);
		if (responses[0])
			*count = 1;
	} else { log_warn("Unknown command %s", cmd); }

	return 0;
}
