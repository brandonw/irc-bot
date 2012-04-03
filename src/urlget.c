#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "string.h"
#include "errno.h"
#include "debug.h"

static char PLUG_NAME[] = "urlget";
static char PLUG_DESCR[] = "Gets title from URLs in chat";
static const int command_qty = 0;

char **get_commands()
{
	return NULL;
}

int get_command_qty()
{
	return command_qty;
}

char *get_plug_name()
{
	return PLUG_NAME;
}

char *get_plug_descr()
{
	return PLUG_DESCR;
}

char **get_plug_help()
{
	return NULL;
}

int plug_init()
{
	return 0;
}

int plug_close()
{
	return 0;
}

int msg_reply(char *src, char *dest, char *msg)
{
	char buf[IRC_BUF_LENGTH];
	char *n;

		/*plug_close();*/
		/*plug_init();*/

		/*sprintf(buf, "%s", "Reloaded map pools");*/
		/*responses[*count] =*/
			/*create_plug_msg(src, buf);*/
		/*if (responses[*count])*/
			/*(*count)++;*/

	return 0;
}
