#include <stdlib.h>
#include <string.h>
#include "bot.h"
#include "debug.h"

#define QUIT_QUIT_CMD "quit"

static char **commands;
static const int command_qty = 1;
static char quit_cmd[] = QUIT_QUIT_CMD;
static char plug_name[] = "quit";

char **get_commands()
{
	return commands;
}

int get_command_qty()
{
	return command_qty;
}

char *get_plug_name()
{
	return plug_name;
}

int plug_init()
{
	commands = malloc(sizeof(*commands) * command_qty);
	commands[0] = quit_cmd;

	return 0;
}

int plug_close()
{
	free(commands);

	return 0;
}

int create_response(struct irc_message *msg,
		struct irc_message **messages, int *msg_count)
{
	char *msg_nick, *msg_message;
	msg_nick = strtok(msg->prefix, "!") + 1;
	msg_message = strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;

	*msg_count = 0;

	if (strcmp(msg_message, CMD_CHAR QUIT_QUIT_CMD) == 0 &&
			strcmp(msg_nick, "brandonw") == 0) {
		log_info("Received `!quit', quitting...");
		kill_bot(0);
		messages[0] = create_message(NULL, "QUIT", NULL);
		if (messages[0])
			*msg_count = 1;
	}
	return 0;
}
