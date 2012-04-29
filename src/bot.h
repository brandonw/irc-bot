#ifndef BOT_H
#define BOT_H

#define   DEFAULT_PORT        	"6667"
#define   IRC_BUF_LENGTH      	512
#define   MAX_PLUGINS         	50
#define   LAG_INTERVAL          300
#define   PING_WAIT_TIME        15
#define   CMD_CHAR              "!"
#define   MAX_CMD_LENGTH        15

extern char *address, *channel, *nick;

struct plug_msg {
	char *dest;
	char *msg;
};

struct plug_msg *create_plug_msg(char *dest, char *msg);
void free_plug_msg(struct plug_msg *msg);
void kill_bot();
void run_bot();
int send_plug_msg(struct plug_msg *msg);

#endif
