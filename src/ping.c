#include <stdlib.h>
#include <string.h>
#include "bot.h"

const char command[] = "PING";

int create_response(struct irc_message *msg, struct irc_message **messages, int *msg_count)
{
  messages[0] = create_message(NULL, "PONG", msg->params);
  *msg_count = 1;
  return 0;
}
