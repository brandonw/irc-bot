#include <stdlib.h>
#include <string.h>
#include "bot.h"

int has_joined = 0;
const char command[] = "MODE";

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	if (!has_joined) {
		has_joined = 1;
		messages[0] = create_message(NULL, "JOIN", channel);
		if (messages[0])
			*msg_count = 1;
	}
	return 0;
}
