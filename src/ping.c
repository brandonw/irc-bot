#include <stdlib.h>
#include <string.h>
#include "bot.h"

static const char command[] = "PING";

char *get_command()
{
	return (char *)command;
}

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	messages[0] = create_message(NULL, "PONG", msg->params);
	if (messages[0])
		*msg_count = 1;
	return 0;
}
