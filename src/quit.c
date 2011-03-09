#include <stdlib.h>
#include <string.h>
#include "bot.h"

static const char command[] = "PRIVMSG";

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	char *msg_nick = strtok(msg->prefix, "!") + 1;
	char *msg_message = strtok(NULL, "") + 1;
	char *tok;

	tok  = strtok(msg_message, " ");
	*msg_count = 0;

	if (strcmp(tok, "!QUIT") == 0 && strcmp(msg_nick, "brandonw") == 0) {
		kill_bot();
		messages[0] = create_message(NULL, "QUIT", NULL);
		*msg_count = 1;
	}
	return 0;
}
