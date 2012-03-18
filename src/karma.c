#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "debug.h"

#define   MAX_KARMA_LINE_LENGTH	100
#define   MAX_NICK_LENGTH	80
#define   KARMA_FILE            "karma.txt"

static GHashTable *karma_hash = NULL;

static const char command[] = "PRIVMSG";

char *get_command()
{
	return (char *)command;
}

int create_response(struct irc_message *msg,
		struct irc_message **messages, int *msg_count)
{
	char buf[IRC_BUF_LENGTH];
	char *msg_message, *tok, *msg_nick;
	long *k;

	strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;
	tok = strtok(msg_message, " ");

	if (strcmp(tok, "!karma") != 0 && strcmp(tok, "!up") != 0 &&
			strcmp(tok, "!down") != 0)
	{
		return 0;
	}

	msg_nick = strtok(NULL, " ");
	if (msg_nick == NULL || strlen(msg_nick) > MAX_NICK_LENGTH) {
		if (msg_nick == NULL)
			log_warn("Missing nick in karma plugin.");
		else
			log_warn("%s is too long of a nick.", msg_nick);
		return 0;
	}

	*msg_count = 0;

	if (strcmp(tok, "!karma") == 0) {
		long tmp_k = 0;
		k = (long *)g_hash_table_lookup(karma_hash, msg_nick);
		if (k != NULL) {
			tmp_k = *k;
		}

		debug("Retrieving %ld karma for %s", tmp_k, msg_nick);

		sprintf(buf, "%s :%s has %ld karma", channel, msg_nick, tmp_k);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	} else if (strcmp(tok, "!up") == 0) {
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

		sprintf(buf, "%s :%s has been upvoted to %ld karma",
			channel, msg_nick, *k);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	} else if (strcmp(tok, "!down") == 0) {
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

		sprintf(buf, "%s :%s has been downvoted to %ld karma",
			channel, msg_nick, *k);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	}

	return 0;
}

int plug_init()
{
	FILE *fp;
	char buf[MAX_KARMA_LINE_LENGTH];
	int errno, lineno;
	char *n;
	long *k;

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
		if (strlen(tok) > MAX_NICK_LENGTH) {
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
