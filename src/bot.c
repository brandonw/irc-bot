#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>

#include "bot.h"

int keep_alive = 1;

int getaddr (struct addrinfo **result);
int connect_to_server();
int send_msg(struct irc_message *message);
int recv_msg(struct irc_message **message);
int sockfd;

void free_message(struct irc_message *message);
int process_message(struct irc_message *msg);
int filter(const struct dirent *);

struct plugin {
  void *handle;
  char *command;
  int (*create_response)(char*, char*, 
                         struct irc_message*[MAX_RESPONSE_MSGES], 
                         int*);
  int (*initialize)();
  int (*close)();
};
static struct plugin plugins[MAX_PLUGINS];
static int num_of_plugins = 0;

int filter(const struct dirent *d)
{
	int len;

	if(strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0)
		return 0;

	len = strlen(d->d_name);

	if(len < 4)
		return 0;

	if(strcmp(d->d_name + len - 3, ".so") == 0)
		return 1;
	
	return 0;
}

int load_plugins()
{
  struct dirent **namelist;
  int n;

  n = scandir("plugins", &namelist, &filter, alphasort);
  if (n < 0) {
    perror("scandir");
    exit(EXIT_FAILURE);
  }

  while (n--) {

    char location[100] = "plugins/";
    strcpy(location+8, namelist[n]->d_name);

    plugins[num_of_plugins].handle = dlopen(location, RTLD_LAZY);
    if (!plugins[num_of_plugins].handle) {
      fprintf(stderr, "%s\n", dlerror());
      exit(EXIT_FAILURE);
    }
    dlerror();

    plugins[num_of_plugins].command = 
        (char*)dlsym(plugins[num_of_plugins].handle, "command");
    *(void **) (&(plugins[num_of_plugins].create_response)) = 
        dlsym(plugins[num_of_plugins].handle, "create_response");
    *(void **) (&(plugins[num_of_plugins].initialize)) = 
        dlsym(plugins[num_of_plugins].handle, "initialize");
    *(void **) (&(plugins[num_of_plugins].close)) = 
        dlsym(plugins[num_of_plugins].handle, "close");

    /* only count this as a valid plugin if both create_response
     * and command were found */
    if (plugins[num_of_plugins].create_response != NULL &&
        plugins[num_of_plugins].command != NULL) {
      num_of_plugins++;
    }

    free(namelist[n]);
  }
  free(namelist);
}

int run_bot() 
{
  int p_index;

  load_plugins();

  for (p_index = 0; p_index < num_of_plugins; p_index++) {
    if (plugins[p_index].initialize != NULL) {
      plugins[p_index].initialize();
    }
  }

  sockfd = connect_to_server();

  int bytes_rcved = 0;
  struct irc_message *inc_msg;
  struct irc_message *out_msg;

  bytes_rcved = recv_msg(&inc_msg);
  do 
  {
    process_message(inc_msg);
    free_message(inc_msg);
    bytes_rcved = recv_msg(&inc_msg);
  } while (keep_alive && bytes_rcved > 0 && inc_msg != NULL);

  if (inc_msg != NULL) 
  {
    free_message(inc_msg);
  }

  shutdown(sockfd, SHUT_RDWR);
  for (p_index = 0; p_index < num_of_plugins; p_index++) {
    if (plugins[p_index].close != NULL) {
      (*(plugins[p_index].close))();
    }
    dlclose(plugins[p_index].handle);
  }
}

int process_message(struct irc_message *msg) 
{
  int p_index;
  struct irc_message *responses[MAX_RESPONSE_MSGES];
  int num_of_responses = 0;
  for (p_index = 0; p_index < num_of_plugins; p_index++) {
    if (strcmp(plugins[p_index].command, msg->command) == 0) {
      char prefix[strlen(msg->prefix) + 1];
      char params[strlen(msg->params) + 1];
      strcpy(prefix, msg->prefix);
      strcpy(params, msg->params);
      (*(plugins[p_index].create_response))(
          prefix, params, responses, &num_of_responses);

      if (num_of_responses > 0) 
      {
        int i;
        for (i = 0; i < num_of_responses; i++) {
          send_msg(responses[i]);
          free_message(responses[i]);
        }
      }
    }
  }
}

int connect_to_server() 
{
  struct addrinfo *addr;
  getaddr(&addr);

  int sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (sockfd < 0) 
  {
    perror("socket");
    exit (EXIT_FAILURE);
  }

  int conn_result;
  conn_result = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
  if (conn_result < 0) 
  {
    printf("Failure connecting to %s", address);
    return -1;
  }
  freeaddrinfo(addr);

  return sockfd;
}

int send_msg(struct irc_message *message) 
{
  static char buf[IRC_BUF_LENGTH];
  memset(buf, 0, sizeof(buf));
  int idx = 0;

  if (message->prefix[0] != '\0') 
  {
    sprintf(buf+idx, "%s ", message->prefix);
    idx += strlen(message->prefix) + 1;
  }

  sprintf(buf+idx, "%s", message->command);
  idx += strlen(message->command);

  if (message->params[0] != '\0') 
  {
    sprintf(buf+idx, " %s", message->params);
    idx += strlen(message->params) + 1;
  }

  sprintf(buf+idx, "\r\n");

  /*printf("S:%s", buf);*/

  return send(sockfd, (void*)buf, strlen(buf), 0);
}

int recv_msg(struct irc_message **message) 
{
  static char buf[IRC_BUF_LENGTH];
  int bytes_rcved = 0;
  memset(buf, 0, sizeof(buf));

  do
    bytes_rcved += recv(sockfd, (void*)(buf + bytes_rcved), 1, 0);
   while (buf[bytes_rcved - 1] != '\n');
  
  if (bytes_rcved <= 0)
    return bytes_rcved;

  /*printf("R:%s", buf);*/

  char *prefix, *command, *params;
  prefix = command = params = NULL;

  char *tok = strtok(buf, " ");
  if (tok[0] != ':') 
  {
    command = tok;
  }
  else 
  {
    prefix = tok;
    command = strtok(NULL, " ");
  }

  if ((tok = strtok(NULL, "\r\n")) != NULL) 
  {
    params = tok;
  }
  
  *message = create_message(prefix, command, params);
  return bytes_rcved;
}

struct irc_message *create_message(char *prefix, 
                                   char *command, 
                                   char *params) 
{
  struct irc_message *msg = malloc(sizeof(struct irc_message));
  memset(msg->prefix, 0, sizeof(msg->prefix));
  memset(msg->command, 0, sizeof(msg->command));
  memset(msg->params, 0, sizeof(msg->params));

  int len = 0;
  if (prefix != NULL) 
  {
    len += strlen(prefix) + 1; /* +1 for space after prefix */
    if (len > IRC_BUF_LENGTH) 
    {
      free_message(msg);
      return NULL;
    }
    strcpy(msg->prefix, prefix);
  }

  len += strlen(command);
  if (len > IRC_BUF_LENGTH) 
  {
    free_message(msg);
    return NULL;
  }
  strcpy(msg->command, command);

  if (params != NULL) 
  {
    len += strlen(params) + 1; /* +1 for space before params */
    if (len > IRC_BUF_LENGTH) 
    {
      free_message(msg);
      return NULL;
    }
    strcpy(msg->params, params);
  }

  return msg;
}

void free_message(struct irc_message *message) 
{
  free(message);
  return;
}

int getaddr (struct addrinfo **result) 
{
  char addr_cpy[strlen(address)];
  strcpy(addr_cpy, address);

  struct addrinfo hints;
  int s;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  char *name, *port;
  name = strtok(addr_cpy, ":");
  port = strtok(NULL, " ");

  s = getaddrinfo(name, 
                  port == NULL ? DEFAULT_PORT : port, 
                  &hints, 
                  result);
  if (s != 0) 
  {
    printf("error getting addrinfo\n");
    exit(EXIT_FAILURE);
  }

  return 0;
}
