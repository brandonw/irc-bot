#include <stdlib.h>
#include <string.h>
#include "bot.h"

int has_joined = 0;
const char command[] = "MODE";

int create_response(char *prefix, char *params,
                           struct irc_message **messages,
                           int *msg_count)
{
  if (!has_joined) {
    char *p = malloc(strlen(channel) + 1);
    strcpy(p, channel);
    has_joined = 1;
    messages[0] = create_message(NULL, "JOIN", p);
    *msg_count = 1;
  }
}
