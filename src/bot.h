#ifndef BOT_H

#define BOT_H

#define   DEFAULT_PORT        "6667"
#define   IRC_BUF_LENGTH      513
#define   MAX_NICK_LENGTH     500
#define   MAX_RESPONSE_MSGES  10
#define   MAX_PLUGINS         50

extern char *address, *channel, *nick;
extern int keep_alive;


/*
 * Message and struct functions
 */

/*
 * This struct should only be used when creating the return value type
 * of a plugin.
 */
struct irc_message 
{
  char prefix[IRC_BUF_LENGTH];
  char command[IRC_BUF_LENGTH];
  char params[IRC_BUF_LENGTH];
};

/*
 * Creates a response irc message. Must pre-allocate memory for all strings.
 */
struct irc_message *create_message(char *prefix, 
                                   char *command, 
                                   char *params);


/*
 * Run bot.
 */
int run_bot();

/*
 * Load plugins.
 */
int load_plugins();


#endif /* end of include guard: BOT_H */
