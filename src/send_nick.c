#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bot.h"

int has_sent_nick = 0;
const char command[] = "NOTICE";

int create_response(char *prefix, char *params,
                    struct irc_message **messages,
                    int *msg_count)
{
  *msg_count = 0;

  if (!has_sent_nick) {
    messages[0] = create_message(NULL, "NICK", nick);
    char buf[IRC_BUF_LENGTH];
    sprintf(buf, "USER %s %s 8 * : %s", nick, nick, nick);
    messages[1] = create_message(NULL, "USER", buf);
    *msg_count = 2;
  }

  return 0;
}
