#include <stdlib.h>
#include <string.h>
#include "bot.h"

const char command[] = "PING";

int create_response(char *prefix, char *params,
                    struct irc_message **messages,
                    int *msg_count)
{
  char *p = malloc(strlen(params) + 1);
  strcpy(p, params);
  messages[0] = create_message(NULL, "PONG", p);
  *msg_count = 1;
  return 0;
}
