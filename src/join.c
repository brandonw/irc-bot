#include <stdlib.h>
#include <string.h>
#include "bot.h"

static const char command[] = "MODE";

char *get_command()
{
	return (char *)command;
}

int create_response(struct irc_message *msg, 
		struct irc_message **messages, int *msg_count)
{
	if (!has_joined()) {
		set_joined();
		messages[0] = create_message(NULL, "JOIN", channel);
		if (messages[0])
			*msg_count = 1;
	}
	return 0;
}
