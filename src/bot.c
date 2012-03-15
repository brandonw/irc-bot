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
#include "bot.h"
#include "debug.h"

struct plugin {
	void *handle;
	char *(*get_command) ();
	int (*create_response) (struct irc_message *,
			struct irc_message **, int *);
	int (*initialize) ();
	int (*close) ();
};

static struct plugin plugins[MAX_PLUGINS];
static int nplugins = 0, keep_alive = 1, sent_nick = 0, joined = 0,
	   waiting_for_ping = 0, sockfd= -1;
static time_t last_activity = 0;

int has_sent_nick()
{
	return sent_nick;
}

int has_joined()
{
	return joined;
}

void set_nick_sent()
{
	sent_nick = 1;
	return;
}

void set_joined()
{
	joined = 1;
	return;
}

void reset()
{
	sent_nick = 0;
	joined = 0;
	return;
}

struct irc_message *create_message(char *prefix, char *command, char *params)
{
	size_t msg_size = 0;
	struct irc_message *msg;

	/* check for total size of potential message */
	if (prefix)
		msg_size += strlen(prefix);
	if (command)
		msg_size += strlen(command);
	if (params)
		msg_size += strlen(params);

	if (msg_size >= IRC_BUF_LENGTH) {
		log_err("Attempted to create a message with:\nprefix:%s\n"
			"command:%s\nparams:%s\nwith a total size of %d",
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

static int send_msg(struct irc_message *message)
{
	char buf[IRC_BUF_LENGTH];
	int idx = 0;

	debug("Sending message--\n"
			"prefix:  \"%s\"\n"
			"command: \"%s\"\n"
			"params:  \"%s\"",
			message->prefix, message->command, message->params);

	if (message->prefix) {
		sprintf(buf + idx, "%s ", message->prefix);
		idx += strlen(message->prefix) + 1;
	}

	sprintf(buf + idx, "%s", message->command);
	idx += strlen(message->command);

	if (message->params) {
		sprintf(buf + idx, " %s", message->params);
		idx += strlen(message->params) + 1;
	}

	sprintf(buf + idx, "\r\n");

	return send(sockfd, buf, strlen(buf), 0);
}

static void process_message(struct irc_message *msg)
{
	int i;
	struct irc_message *responses[MAX_RESPONSE_MSGES];
	int num_of_responses;

	for (i = 0; i < nplugins; i++) {
		struct irc_message *temp_msg;

		num_of_responses = 0;
		if (strcmp(plugins[i].get_command(), msg->command))
			continue;

		temp_msg =
		    create_message(msg->prefix, msg->command, msg->params);

		if (!temp_msg)
			continue;

		plugins[i].create_response(temp_msg, responses,
				&num_of_responses);
		free_message(temp_msg);

		if (num_of_responses > 0) {
			int i;
			debug("Plugin acting on this message.");
			for (i = 0; i < num_of_responses; i++) {
				send_msg(responses[i]);
				free_message(responses[i]);
			}
		}
	}
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

static struct irc_message *recv_msg()
{
	char buf[IRC_BUF_LENGTH];
	int bytes_rcved = 0, retval = 0;
	char *prefix, *command, *params, *tok;
	fd_set rfds;
	struct timeval tv;
	struct irc_message *msg;

	if (sockfd == -1) {
		if (time(NULL) - last_activity > LAG_INTERVAL) {
			log_info("Trying to reconnect...");
			sockfd = connect_to_server();
			if (sockfd == -1) {
				log_info("Failed");
				return NULL;
			} else
				log_info("Succeeded!");
		}
		else {
			sleep(5);
			return NULL;
		}
	}

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);
	if (retval == -1) {
		kill_bot(0);
		return NULL;
	} else if (!retval) {
		time_t curr;
		time(&curr);

		if (curr - last_activity > LAG_INTERVAL) {
			if (!waiting_for_ping) {
				struct irc_message *ping_msg;
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
		return NULL;
	}

	do {
		int bytes_read;

		bytes_read = recv(sockfd, buf + bytes_rcved, 1, 0);
		if (bytes_read == 0) {
			log_info("Connection closed.");
			kill_bot(0);
			return NULL;
		}

		if (bytes_read == -1) {
			log_err("Error receiving packets: %s",
					strerror(errno));
			kill_bot(1);
			return NULL;
		}

		bytes_rcved += bytes_read;
	} while (buf[bytes_rcved - 1] != '\n');

	buf[bytes_rcved] = '\0';
	time(&last_activity);
	waiting_for_ping = 0;

	prefix = command = params = NULL;

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

	msg = create_message(prefix, command, params);
	debug("Received message--\n"
			"prefix:  \"%s\"\n"
			"command: \"%s\"\n"
			"params:  \"%s\"",
			msg->prefix, msg->command, msg->params);

	return msg;
}

static void load_plugins()
{
	struct dirent **namelist;
	void *handle;
	int n;

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

		plugins[nplugins].handle = handle;

		plugins[nplugins].get_command = dlsym(handle, "get_command");
		plugins[nplugins].create_response =
		    dlsym(handle, "create_response");
		plugins[nplugins].initialize = dlsym(handle, "initialize");
		plugins[nplugins].close = dlsym(handle, "close");

		/* only count this as a valid plugin if both create_response
		 * and get_command were found */
		if (plugins[nplugins].create_response && plugins[nplugins].get_command) {
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
	int p_index;
	struct irc_message *inc_msg;

	load_plugins();

	for (p_index = 0; p_index < nplugins; p_index++) {
		if (plugins[p_index].initialize)
			plugins[p_index].initialize();
	}

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

	for (p_index = 0; p_index < nplugins; p_index++) {
		if (plugins[p_index].close) {
			plugins[p_index].close();
		}
		dlclose(plugins[p_index].handle);
	}
}

void kill_bot(int p)
{
	debug("Killing bot...");
	keep_alive = 0;
}
