#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"

#define   MAX_KARMA_LINE_LENGTH	100
#define   MAX_NICK_LENGTH	80

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
	char *msg_message, *tok, *nick, *n;
	long *karma;
	long k = 0;

	strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;
	tok = strtok(msg_message, " ");

	n = strtok(NULL, " ");
	if (n == NULL || strlen(n) > MAX_NICK_LENGTH)
		return 0;

	*msg_count = 0;

	if (strcmp(tok, "!karma") == 0) {
		karma = (long *)g_hash_table_lookup(karma_hash, n);
		if (karma != NULL) {
			k = *karma;
		}

		sprintf(buf, "%s :%s has %ld karma", channel, n, k);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	} else if (strcmp(tok, "!up") == 0) {
		karma = (long *)g_hash_table_lookup(karma_hash, n);
		if (karma == NULL) {
			karma = malloc(sizeof(*karma));
			*karma = 0;
			nick = strdup(n);
			g_hash_table_insert(karma_hash, nick, karma);
		}
		(*karma)++;

		sprintf(buf, "%s :%s has been upvoted to %ld karma",
			channel, n, *karma);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	} else if (strcmp(tok, "!down") == 0) {
		karma = (long *)g_hash_table_lookup(karma_hash, n);
		if (karma == NULL) {
			karma = malloc(sizeof(*karma));
			*karma = 0;
			nick = strdup(n);
			g_hash_table_insert(karma_hash, n, karma);
		}
		(*karma)--;

		sprintf(buf, "%s :%s has been downvoted to %ld karma",
			channel, n, *karma);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		if (messages[0])
			*msg_count = 1;
	}

	return 0;
}

int initialize()
{
	FILE *fp;
	char buf[MAX_KARMA_LINE_LENGTH];
	int errno;
	char *n;
	long *k;

	karma_hash = g_hash_table_new(g_str_hash, g_str_equal);

	fp = fopen("karma.txt", "r");
	if (!fp)
		return 0;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *tok, *endptr;

		if (strrchr(buf, '\n') == NULL)
			continue;

		tok = strtok(buf, "\t ");
		if (tok == NULL)
			continue;
		if (strlen(tok) > MAX_NICK_LENGTH) {
			fprintf(stderr, "Nick is too long.\n");
			continue;
		}
		n = strdup(tok);

		tok = strtok(NULL, "\t ");
		if (tok == NULL)
		{
			free(n);
			continue;
		}
		k = (long *)malloc(sizeof(*k));
		errno = 0;
		*k = strtol(tok, &endptr, 10);

		if ((errno == ERANGE && (*k == LONG_MAX || *k == LONG_MIN))
				|| (errno != 0 && *k == 0)) {
			fprintf(stderr, "Karma outside of range.\n");
			continue;
		}
		if (endptr == tok) {
			fprintf(stderr, "Invalid karma format.\n");
			continue;
		}

		g_hash_table_insert(karma_hash, (gpointer) n, (gpointer) k);
	}
	fclose(fp);
	return 0;
}

int close()
{
	GList *key_list;
	GList *keys;
	FILE *fp;
	char *nick;
	long *karma;

	key_list = g_hash_table_get_keys(karma_hash);
	keys = key_list;
	fp = fopen("karma.txt", "w");
	if (fp == NULL)
		fprintf(stderr, "Error opening karma.txt\n");

	while (keys != NULL) {
		nick = (char *)keys->data;
		karma = (long *)g_hash_table_lookup(karma_hash,
				(gconstpointer) nick);
		if (fp != NULL)
			fprintf(fp, "%s\t%ld\n", nick, *karma);

		g_hash_table_remove(karma_hash, (gconstpointer) nick);
		free(nick);
		free(karma);
		keys = g_list_next(keys);
	}
	g_list_free(key_list);
	g_hash_table_destroy(karma_hash);
	fclose(fp);
	return 0;
}
