#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "bot.h"

static GHashTable *karma_hash = NULL;

const char command[] = "PRIVMSG";

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	char buf[IRC_BUF_LENGTH];
	char *msg_message, *tok, *nick;
	int *karma;
	int k = 0;

	strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;
	tok = strtok(msg_message, " ");

	*msg_count = 0;

	if (strcmp(tok, "!karma") == 0) {
		tok = strtok(NULL, " ");
		karma = (int *)g_hash_table_lookup(karma_hash, tok);
		if (karma != NULL) {
			k = *karma;
		}

		sprintf(buf, "%s :%s has %d karma", channel, tok, k);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		*msg_count = 1;
	} else if (strcmp(tok, "!up") == 0) {
		tok = strtok(NULL, " ");
		karma = (int *)g_hash_table_lookup(karma_hash, tok);
		if (karma == NULL) {
			karma = malloc(sizeof(int));
			*karma = 0;
			nick = malloc(strlen(tok) + 1);
			strcpy(nick, tok);
			g_hash_table_insert(karma_hash, nick, karma);
		}
		(*karma)++;

		sprintf(buf, "%s :%s has been upvoted to %d karma",
			channel, tok, *karma);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		*msg_count = 1;
	} else if (strcmp(tok, "!down") == 0) {
		tok = strtok(NULL, " ");
		karma = (int *)g_hash_table_lookup(karma_hash, tok);
		if (karma == NULL) {
			karma = malloc(sizeof(int));
			*karma = 0;
			nick = malloc(strlen(tok) + 1);
			strcpy(nick, tok);
			g_hash_table_insert(karma_hash, nick, karma);
		}
		(*karma)--;

		sprintf(buf, "%s :%s has been downvoted to %d karma",
			channel, tok, *karma);
		messages[0] = create_message(NULL, "PRIVMSG", buf);
		*msg_count = 1;
	}

	return 0;
}

int initialize()
{
	FILE *fp;
	char nick[MAX_NICK_LENGTH];
	int karma;
	char *n;
	int *k;

	karma_hash = g_hash_table_new(g_str_hash, g_str_equal);

	fp = fopen("karma.txt", "a");
	if (!fp)
		return 0;

	while (fscanf(fp, "%s\t%d\n", nick, &karma) != EOF) {
		n = (char *)malloc(strlen(nick) + 1);
		k = (int *)malloc(sizeof(int));
		strcpy(n, nick);
		*k = karma;
		g_hash_table_insert(karma_hash, (gpointer) n, (gpointer) k);
	}
	fclose(fp);
	return 0;
}

int close()
{
	GList *key_list = g_hash_table_get_keys(karma_hash);
	GList *keys = key_list;
	FILE *fp = fopen("karma.txt", "w");
	char *nick;
	int *karma;
	while (keys != NULL) {
		nick = (char *)keys->data;
		karma =
		    (int *)g_hash_table_lookup(karma_hash,
					       (gconstpointer) nick);
		fprintf(fp, "%s\t%d\n", nick, *karma);
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
