#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>
#include <curl/curl.h>
#include "bot.h"
#include "debug.h"

#define RECV_BUF_LENGTH IRC_BUF_LENGTH * 5

struct plugin {
	void *handle;
	char **(*get_commands) ();
	int (*get_command_qty) ();
	int (*cmd_reply) (char *src, char *dest, char *cmd, char *msg);
	int (*msg_reply) (char *src, char *dest, char *msg);
	int (*plug_init) ();
	int (*plug_close) ();
	char *(*get_plug_name) ();
	char *(*get_plug_descr) ();
	char **(*get_plug_help) ();
};

struct irc_message {
	char *prefix;
	char *command;
	char *params;
};

static struct plugin plugins[MAX_PLUGINS];
static int nplugins = 0, keep_alive = 1, sent_nick = 0, joined = 0,
	   waiting_for_ping = 0, sockfd= -1;
static time_t last_activity = 0;
static char recv_buf[RECV_BUF_LENGTH - 1];
static ssize_t recv_buf_size = 0;
static char *recv_buf_pos = NULL;

void reset()
{
	sent_nick = 0;
	joined = 0;
	return;
}

void kill_bot()
{
	debug("Killing bot...");
	keep_alive = 0;
}

struct plug_msg *create_plug_msg(char *dest, char *msg)
{
	struct plug_msg *pmsg;

	pmsg = malloc(sizeof(*pmsg));
	pmsg->dest = strdup(dest);
	pmsg->msg = strdup(msg);

	return pmsg;
}

void free_plug_msg(struct plug_msg *msg)
{
	if (msg) {
		free(msg->dest);
		free(msg->msg);
		free(msg);
	}
}

struct irc_message *create_message(char *prefix, char *command, char *params)
{
	size_t msg_size = 0;
	struct irc_message *msg;

	/* check for total size of potential message */
	if (prefix)
		msg_size += strlen(prefix);
	if (!command) {
		log_err("Attempted to create a message with no command.\n");
		return NULL;
	}
	msg_size += strlen(command);
	if (params)
		msg_size += strlen(params);

	if (msg_size >= IRC_BUF_LENGTH) {
		log_err("Attempted to create a message with:\n"
			"prefix:%s\n"
			"command:%s\n"
			"params:%s\n"
			"with a total size of %d\n",
			prefix,
			command,
			params,
			(int)msg_size);
		return NULL;
	}

	msg = malloc(sizeof(*msg));
	msg->prefix = NULL;
	msg->command = NULL;
	msg->params = NULL;

	if (prefix)
		msg->prefix = strdup(prefix);

	msg->command = strdup(command);

	if (params)
		msg->params = strdup(params);

	return msg;
}

void free_message(struct irc_message *message)
{
	if (message->prefix)
		free(message->prefix);
	if (message->params)
		free(message->params);
	if (message->command)
		free(message->command);
	free(message);
}

static int filter(const struct dirent *d)
{
	int len;

	if (strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0)
		return 0;

	len = strlen(d->d_name);

	if (len < 4)
		return 0;

	if (strcmp(d->d_name + len - 3, ".so") == 0)
		return 1;

	return 0;
}

static int plugin_accepts_command(struct plugin *p, char *cmd)
{
	int cmd_qty = 0, i;
	char **commands;

	cmd_qty = p->get_command_qty();
	commands = p->get_commands();

	for (i = 0; i < cmd_qty; i++) {
		if (!strcmp(cmd, commands[i])) {
			debug("%s plugin acting on it.", p->get_plug_name());
			return 1;
		}
	}
	return 0;
}

static int send_msg(struct irc_message *message)
{
	char buf[IRC_BUF_LENGTH];
	int idx = 0;

	debug("\n%%Sending message--\n"
	      "%%prefix:  \"%s\"\n"
	      "%%command: \"%s\"\n"
	      "%%params:  \"%s\"",
	      message->prefix, message->command, message->params);

	if (message->prefix) {
		snprintf(buf + idx, IRC_BUF_LENGTH, "%s ", message->prefix);
		idx += strlen(message->prefix) + 1;
	}

	snprintf(buf + idx, IRC_BUF_LENGTH, "%s", message->command);
	idx += strlen(message->command);

	if (message->params) {
		snprintf(buf + idx, IRC_BUF_LENGTH, " %s", message->params);
		idx += strlen(message->params) + 1;
	}

	snprintf(buf + idx, IRC_BUF_LENGTH, "\r\n");

	return send(sockfd, buf, strlen(buf), 0);
}

int send_plug_msg(struct plug_msg *msg)
{
	char buf[IRC_BUF_LENGTH];
	struct irc_message *tmp;

	snprintf(buf, IRC_BUF_LENGTH, "%s :%s", msg->dest, msg->msg);
	tmp = create_message(NULL, "PRIVMSG", buf);
	if (tmp) {
		send_msg(tmp);
		free_message(tmp);
	}

	return 1;
}

static void send_join()
{
	struct irc_message *join_msg;
	log_info("Sending JOIN message.");
	join_msg = create_message(NULL, "JOIN", channel);
	if (join_msg) {
		send_msg(join_msg);
		free_message(join_msg);
	}

	joined = 1;
}

static void send_nick()
{
	struct irc_message *id_msg;
	char buf[IRC_BUF_LENGTH];

	log_info("Received NOTICE indicating connection is active, "
			"sending NICK and USER information.");

	id_msg = create_message(NULL, "NICK", nick);
	if (id_msg) {
		send_msg(id_msg);
		free_message(id_msg);
	}

	snprintf(buf, IRC_BUF_LENGTH, "USER %s %s 8 * : %s", nick, nick, nick);
	id_msg = create_message(NULL, "USER", buf);
	if (id_msg) {
		send_msg(id_msg);
		free_message(id_msg);
	}

	sent_nick = 1;
}

static void send_ping(char *ping)
{
	struct irc_message *pong_msg;

	log_info("Received PING, sending PONG.");
	pong_msg = create_message(NULL, "PONG", ping);
	if (pong_msg) {
		send_msg(pong_msg);
		free_message(pong_msg);
	}
}

static void print_help_msges(char *src)
{
	int i, j;
	char buf[IRC_BUF_LENGTH];
	struct plugin *p;
	struct irc_message *tmp;

	snprintf(buf, IRC_BUF_LENGTH, "%s :The following commands are available:", src);
	tmp = create_message(NULL, "PRIVMSG", buf);
	if (tmp) {
		send_msg(tmp);
		free_message(tmp);
	}

	for (i = 0; i < nplugins; i++) {
		char *name = NULL, *descr = NULL;
		p = &plugins[i];

		snprintf(buf, IRC_BUF_LENGTH, "%s : ", src);
		tmp = create_message(NULL, "PRIVMSG", buf);
		if (tmp) {
			send_msg(tmp);
			free_message(tmp);
		}

		name = p->get_plug_name();
		if (p->get_plug_descr)
			descr = p->get_plug_descr();

		snprintf(buf, IRC_BUF_LENGTH, "%s :%s%s%s", src, name,
				(descr ? " - " : ""),
				(descr ? descr : ""));
		tmp = create_message(NULL, "PRIVMSG", buf);
		if (tmp) {
			send_msg(tmp);
			free_message(tmp);
		}

		for (j = 0; j < p->get_command_qty(); j++) {
			char *cmd = NULL, *help = NULL;
			cmd = p->get_commands()[j];
			if (p->get_plug_help)
				help = p->get_plug_help()[j];

			snprintf(buf, IRC_BUF_LENGTH, "%s :     %s%s%s%s",
					src, CMD_CHAR, cmd,
					(help ? " - " : ""),
					(help ? help : ""));
			tmp = create_message(NULL, "PRIVMSG", buf);
			if (tmp) {
				send_msg(tmp);
				free_message(tmp);
			}
		}
	}

	snprintf(buf, IRC_BUF_LENGTH, "%s : ", src);
	tmp = create_message(NULL, "PRIVMSG", buf);
	if (tmp) {
		send_msg(tmp);
		free_message(tmp);
	}

}

static void send_quit()
{
	struct irc_message *quit_msg;
	log_info("Received `!quit', quitting...");
	kill_bot();
	quit_msg = create_message(NULL, "QUIT", NULL);
	if (quit_msg) {
		send_msg(quit_msg);
		free_message(quit_msg);
	}
}

static void process_pm_cmd(char *cmd, char *src, char *dest, char *msg)
{
	int i;
	struct plugin *p;

	if (!strcmp("help", cmd)) {
		print_help_msges(src);
		return;
	} else if (!strcmp("quit", cmd)) {
		char *src_nick = strtok(src, "!");
		if (!strcmp("brandonw", src_nick)) {
			send_quit();
		}
		return;
	}

	for (i = 0; i < nplugins; i++) {
		p = &plugins[i];
		debug("plug name: %s", p->get_plug_name());
		if (!plugin_accepts_command(p, cmd))
			continue;

		if (!p->cmd_reply) {
			log_warn("%s plugin accepts command %s but does not"
					"implement cmd_reply",
					p->get_plug_name(), cmd);
			return;
		}
		if (p->cmd_reply(src, dest, cmd, msg)) {
			return;
		}

		return;
	}
}

static void process_pm_msg(char *src, char *dest, char *msg)
{
	int i;
	struct plugin *p;

	for (i = 0; i < nplugins; i++) {
		p = &plugins[i];
		debug("plug name: %s", p->get_plug_name());
		if (!p->msg_reply)
			continue;

		if (p->msg_reply(src, dest, msg)) {
			return;
		}
	}
}

static void process_priv_message(struct irc_message *irc_msg)
{
	char *cmd, *dest, *msg, *src;

	dest = strtok(irc_msg->params, " ");
	msg = strtok(NULL, "");
	if (*msg == ':') { // ignore irc message : prefix
		msg = msg + 1;
	}
	src = strtok(irc_msg->prefix + 1, "!");

	if (*msg == *CMD_CHAR) {
		cmd = strtok(msg, " ") + 1; // ignore CMD_CHAR char
		msg = strtok(NULL, "");
		debug("Command received: %s", cmd);
		process_pm_cmd(cmd, src, dest, msg);
	} else {
		process_pm_msg(src, dest, msg);

	}
}

static void process_message(struct irc_message *msg)
{
	if (!joined && !strcmp("001", msg->command)) {
		send_join();
		return;
	} else if (!sent_nick && !strcmp("NOTICE", msg->command)) {
		send_nick();
		return;
	} else if (!strcmp("PING", msg->command)) {
		send_ping(msg->params);
		return;
	} else if (strcmp("PRIVMSG", msg->command)) {
		return;
	}

	process_priv_message(msg);
}

static int getaddr(struct addrinfo **result)
{
	char *name, *port, *addr;
	struct addrinfo hints, *p;
	int s, sockfd;

	addr = strdup(address);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	name = strtok(addr, ":");
	port = strtok(NULL, " ");

	if ((s = getaddrinfo(name,
			     port == NULL ? DEFAULT_PORT : port,
			     &hints,
			     result)) != 0) {
		log_err("Error retrieving address info: %s", gai_strerror(s));
		free(addr);
		return -1;
	}

	for (p = *result; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				     p->ai_protocol)) == -1) {
			log_err("Error creating socket: %s",
					strerror(errno));
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			log_err("Error connecting to server: %s",
					strerror(errno));
			continue;
		}

		break; /* successfully connected */
	}

	if (p == NULL) {
		log_err("Failed to connect.");
		free(addr);
		exit(2);
	}

	*result = p;
	free(addr);
	return 0;
}

static int connect_to_server()
{
	struct addrinfo *addr;
	int fd, getaddr_res;
	int conn_result;

	time(&last_activity);

	getaddr_res = getaddr(&addr);
	if (getaddr_res == -1)
		return -1;

	fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (fd < 0) {
		log_err("Error creating socket: %s", strerror(errno));
		freeaddrinfo(addr);
		exit(EXIT_FAILURE);
	}

	conn_result = connect(fd, addr->ai_addr, addr->ai_addrlen);
	if (conn_result < 0) {
		log_err("Error connecting to server: %s",
				strerror(errno));
		freeaddrinfo(addr);
		return -1;
	}
	freeaddrinfo(addr);

	return fd;
}

static int fill_recv_buf()
{
	int retval = 0;
	fd_set rfds;
	struct timeval tv;

	if (sockfd == -1) {
		debug("sockfd is -1");
		if (time(NULL) - last_activity > LAG_INTERVAL) {
			debug("LAG_INTERVAL has been reached");
			log_info("Trying to reconnect...");
			sockfd = connect_to_server();
			if (sockfd == -1) {
				log_info("Failed: %s", strerror(errno));
				return -1;
			} else
				log_info("Succeeded!");
		}
		else {
			debug("LAG_INTERVAL not reached; sleeping...");
			sleep(5);
			return -1;
		}
	}

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
	if (retval == -1) {
		log_err("Error waiting for socket: %s", strerror(errno));
		close(sockfd);
		sockfd = -1;
		return -1;
	} else if (!retval) {
		time_t curr;
		time(&curr);

		if (curr - last_activity > LAG_INTERVAL) {
			debug("appear to have lost connection");
			if (!waiting_for_ping) {
				struct irc_message *ping_msg;
				debug("sending ping..");
				ping_msg = create_message(NULL, "PING",
						":ping");
				send_msg(ping_msg);
				free_message(ping_msg);
				waiting_for_ping = 1;
			}
			else if (curr - last_activity >
					LAG_INTERVAL + PING_WAIT_TIME) {
				log_info("Lost connection...");
				waiting_for_ping = 0;
				close(sockfd);
				sockfd = -1;
				reset();
			}
		}
		return -1;
	}

	recv_buf_size = recv(sockfd, recv_buf, RECV_BUF_LENGTH - 1, 0);
	if (recv_buf_size == 0) {
		// connection has been terminated
		// likely due to ping timeout
		sockfd = -1;
		log_info("Connection terminated. Reconnecting..");
		return -1;
	} else if (recv_buf_size == -1) {
		log_err("Error receiving packets: %s",
				strerror(errno));
		kill_bot();
		return -1;
	}

	recv_buf[recv_buf_size] = '\0';
	recv_buf_pos = recv_buf;

	time(&last_activity);
	waiting_for_ping = 0;
	return 0;
}

// copies string starting at ptr until it reaches \n or \0 into buf
// returns  int of index of final character copied
// assumes buf is always large enough to hold entire line
// returns -1 if n was reached before finding \n or \0
static int copy_into(char *ptr, char *buf, size_t n)
{
	size_t i = 0;

	do {
		buf[i] = ptr[i];
		i++;
	} while (ptr[i-1] != '\n' && ptr[i-1] != '\0' && i < n);

	if (ptr[i-1] == '\n' || ptr[i-1] == '\0')
		return i-1;
	else
		return -1;
}

static struct irc_message *recv_msg()
{
	char buf[IRC_BUF_LENGTH];
	char *prefix = NULL, *command = NULL, *params = NULL, *tok;
	struct irc_message *irc_msg;
	int i, bo = 0;

	do
	{
		if (*recv_buf_pos == '\0' && fill_recv_buf())
			return NULL;

		i = copy_into(recv_buf_pos, buf + bo, IRC_BUF_LENGTH - bo);
		if (i == -1) {
			log_err("Message received that exceeds IRC message "
					"length [%d].", IRC_BUF_LENGTH);
			return NULL;

		}
		recv_buf_pos += i;
		bo += i;
	} while (buf[bo] != '\n');

	recv_buf_pos++;
	buf[bo+1] = '\0';

	tok = strtok(buf, " ");
	if (tok[0] != ':') {
		command = tok;
	} else {
		prefix = tok;
		command = strtok(NULL, " ");
	}

	if ((tok = strtok(NULL, "\r\n")) != NULL) {
		params = tok;
	}

	irc_msg = create_message(prefix, command, params);
	debug("%%Received message--\n"
	      "%%prefix:  \"%s\"\n"
	      "%%command: \"%s\"\n"
	      "%%params:  \"%s\"",
	      irc_msg->prefix, irc_msg->command, irc_msg->params);

	return irc_msg;
}

static void load_plugins()
{
	struct dirent **namelist;
	void *handle;
	int n;
	struct plugin *p;

	debug("Loading plugins.");

	n = scandir("plugins", &namelist, &filter, alphasort);
	if (n < 0) {
		log_err("Error scanning for plugins: %s",
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	while (n-- && nplugins < MAX_PLUGINS) {
		char location[100] = "plugins/";
		strcpy(location + 8, namelist[n]->d_name);

		debug("Testing %s for plugin.", location);

		handle = dlopen(location, RTLD_LAZY);

		if (!handle) {
			log_err("Error dynamically linking %s: %s",
					location,
					dlerror());
			exit(EXIT_FAILURE);
		}

		p = &plugins[nplugins];

		p->handle = handle;
		p->get_commands = dlsym(handle, "get_commands");
		p->get_command_qty = dlsym(handle, "get_command_qty");
		p->cmd_reply = dlsym(handle, "cmd_reply");
		p->msg_reply = dlsym(handle, "msg_reply");
		p->plug_init = dlsym(handle, "plug_init");
		p->plug_close = dlsym(handle, "plug_close");
		p->get_plug_name = dlsym(handle, "get_plug_name");
		p->get_plug_descr = dlsym(handle, "get_plug_descr");
		p->get_plug_help = dlsym(handle, "get_plug_help");

		if ((p->cmd_reply || p->msg_reply) &&
				p->get_commands &&
				p->get_command_qty &&
				p->plug_init &&
				p->plug_close &&
				p->get_plug_name) {
			log_info("Loaded plugin file %s", location);
			nplugins++;
		}

		free(namelist[n]);
	}

	if (nplugins == MAX_PLUGINS)
		log_warn("Attempted to load too many plugins.");

	free(namelist);
}

void run_bot()
{
	int i;
	struct irc_message *inc_msg;

	curl_global_init(CURL_GLOBAL_SSL);
	load_plugins();

	for (i = 0; i < nplugins; i++) {
		if (plugins[i].plug_init)
			plugins[i].plug_init();
	}

	recv_buf[0] = '\0';
	recv_buf_pos = &recv_buf[0];
	sockfd = connect_to_server();

	while (keep_alive) {
		inc_msg = recv_msg(sockfd);
		if(!inc_msg) {
			continue;
		}

		process_message(inc_msg);
		free_message(inc_msg);
	}

	debug("Cleaning up memory.");

	close(sockfd);

	for (i = 0; i < nplugins; i++) {
		if (plugins[i].plug_close) {
			plugins[i].plug_close();
		}
		dlclose(plugins[i].handle);
	}
	curl_global_cleanup();
}
