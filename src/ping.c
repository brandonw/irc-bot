#include <stdlib.h>
#include <string.h>
#include "bot.h"

const char command[] = "PING";

int create_response(char *prefix, char *params,
                    struct irc_message *messages[MAX_RESPONSE_MSGES],
                    int *msg_count)
{
  messages[0] = create_message(NULL, "PONG", params);
  *msg_count = 1;
  return 0;
}
