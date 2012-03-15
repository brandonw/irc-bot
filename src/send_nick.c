#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bot.h"
#include "debug.h"

static const char command[] = "NOTICE";

char *get_command()
{
	return (char *)command;
}

int create_response(struct irc_message *msg,
		struct irc_message **messages, int *msg_count)
{
	char buf[IRC_BUF_LENGTH];

	if (has_sent_nick())
		return 0;

	log_info("Received NOTICE indicating connection is active, sending "
			"NICK and USER information.");

	messages[0] = create_message(NULL, "NICK", nick);

	sprintf(buf, "USER %s %s 8 * : %s", nick, nick, nick);
	messages[1] = create_message(NULL, "USER", buf);

	if (!messages[0] || !messages[1]) {
		if (messages[0]) {
			free_message(messages[0]);
			log_err("Failed to create NICK message.");

		}
		if (messages[1]) {
			free_message(messages[1]);
			log_err("Failed to create USER message.");
		}
		return -1;
	}

	set_nick_sent();
	(*msg_count) = 2;
	return 0;
}
