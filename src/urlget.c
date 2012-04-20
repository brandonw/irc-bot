#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <curl/curl.h>
#include "bot.h"
#include "string.h"
#include "errno.h"
#include "debug.h"

#define MAX_TITLE_LENGTH IRC_BUF_LENGTH - 50
#define HTML_BUF_SIZE    1024
#define CURL_INIT_FLAGS  (1<<0)
#define CURL_URL_OPT 10002
#define CURL_WRITEDATA_OPT 10001

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
	char *n, *tok, *tmp = NULL, *link = NULL, *title = NULL;
	void *handle;
	char tmphtml[HTML_BUF_SIZE];
	int i, len;
	FILE *tmpf;
	struct plug_msg *tmp_msg;

	if (dest[0] != '#')
		return 0;

	len = strlen(msg);
	for (i = 0; i < len; i++) {
		if (msg[i] == 'h' && (i + 10) < len &&
				(!strncmp("http://", msg + i, 7) ||
				 !strncmp("https://", msg + i, 8))) {
			tmp = msg + i;
			break;
		}
	}

	if (tmp == NULL)
		return 0;
	link = strtok(tmp, " ");
	debug("Found link %s", link);

	if ((tmpf = tmpfile()) == NULL) {
		log_err("%s", strerror(errno));
		return -1;
	}

	link = strtok(link, " ");
	if (!(handle = curl_easy_init())) {
		log_err("Error creating curl handle");
		fclose(tmpf);
		return -1;
	}
	if (curl_easy_setopt(handle, CURL_URL_OPT, link)) {
		log_err("Error setting URL");
		curl_easy_cleanup(handle);
		fclose(tmpf);
		return -1;
	}
	if (curl_easy_setopt(handle, CURL_WRITEDATA_OPT, tmpf)) {
		log_err("Error setting tmpfile");
		curl_easy_cleanup(handle);
		fclose(tmpf);
		return -1;
	}
	if (curl_easy_perform(handle)) {
		log_err("Error executing URL retrieval");
		curl_easy_cleanup(handle);
		fclose(tmpf);
		return -1;
	}

	curl_easy_cleanup(handle);

	fseek(tmpf, 0, SEEK_SET);
	while (fgets(tmphtml, sizeof(*tmphtml) * HTML_BUF_SIZE, tmpf)) {
		if (!strtok(tmphtml, "<"))
			continue;

		do {
			if (!(tok = strtok(NULL, ">")))
				break;

			if (!strncmp("title", tok, 5) &&
					(tok = strtok(NULL, "<"))) {
				title = strdup(tok);
				debug("Found title: %s", title);
				if (strlen(title) > MAX_TITLE_LENGTH)
					title[MAX_TITLE_LENGTH] = '\0';
				break;
			}
		} while (strtok(NULL, "<"));
	}
	fclose(tmpf);

	if (title) {
		tmp_msg = create_plug_msg(dest, title);
		send_plug_msg(tmp_msg);
		free_plug_msg(tmp_msg);
		free(title);
	}
	return 0;
}
