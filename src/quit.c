#include <stdlib.h>
#include <string.h>
#include "bot.h"

const char command[] = "PRIVMSG";

int create_response(struct irc_message *msg, struct irc_message **messages, int *msg_count)
{
  *msg_count = 0;
  char *msg_nick = strtok(msg->prefix, "!")+1;
  char *msg_channel = strtok(msg->params, " "); 
  char *msg_message = strtok(NULL, "")+1;
  char buf[IRC_BUF_LENGTH];
  char *tok = strtok(msg_message, " ");
  if (strcmp(tok, "!QUIT") == 0 && strcmp(msg_nick, "brandonw") == 0) 
  {
    keep_alive = 0;
    messages[0] = create_message(NULL, "QUIT", NULL);
    *msg_count = 1;
  }
  return 0;
}
