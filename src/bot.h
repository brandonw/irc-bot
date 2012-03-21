#ifndef BOT_H
#define BOT_H

#define   DEFAULT_PORT        	"6667"
#define   IRC_BUF_LENGTH      	513
#define   MAX_RESPONSE_MSGES  	10
#define   MAX_PLUGINS         	50
#define   LAG_INTERVAL          300
#define   PING_WAIT_TIME        15
#define   CMD_CHAR              "!"
#define   MAX_CMD_LENGTH        15

extern char *address, *channel, *nick;

struct irc_message {
	char *prefix;
	char *command;
	char *params;
};

void run_bot();
void kill_bot(int p);
struct irc_message *create_message(char *, char *, char *);
void free_message(struct irc_message *message);

#endif
