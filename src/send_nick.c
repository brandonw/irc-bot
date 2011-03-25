#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bot.h"

int has_sent_nick = 0;
static const char command[] = "NOTICE";

char *get_command()
{
	return (char *)command;
}

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	char buf[IRC_BUF_LENGTH];

	if (has_sent_nick)
		return 0;

	messages[0] = create_message(NULL, "NICK", nick);
	if (messages[0])
		(*msg_count)++;
	sprintf(buf, "USER %s %s 8 * : %s", nick, nick, nick);
	messages[1] = create_message(NULL, "USER", buf);
	if (messages[1])
		(*msg_count)++;

	return 0;
}
