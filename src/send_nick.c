#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bot.h"

int has_sent_nick = 0;
const char command[] = "NOTICE";

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	char buf[IRC_BUF_LENGTH];

	if (has_sent_nick)
		return 0;

	messages[0] = create_message(NULL, "NICK", nick);
	sprintf(buf, "USER %s %s 8 * : %s", nick, nick, nick);
	messages[1] = create_message(NULL, "USER", buf);
	*msg_count = 2;

	return 0;
}
