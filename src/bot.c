#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "bot.h"

int getaddr (struct addrinfo **result, char *address);
int connect_to_server(char *address);
int send_msg(struct irc_message *message);
int recv_msg(struct irc_message **message);
int sockfd;

void free_message(struct irc_message *message);
int process_message(struct irc_message *msg);


int run_bot(char *address, char *nick, char *channel) 
{
  sockfd = connect_to_server(address);

  int bytes_rcved = 0;
  struct irc_message *inc_msg;
  struct irc_message *out_msg;

  bytes_rcved = recv_msg(&inc_msg);
  do 
  {
    process_message(inc_msg);
    if (out_msg != NULL) 
    {
      send_msg(out_msg);
      free_message(out_msg);
    }
    free_message(inc_msg);
    bytes_rcved = recv_msg(&inc_msg);
  } while (keep_alive && bytes_rcved > 0 && inc_msg != NULL);

  if (inc_msg != NULL) 
  {
    free_message(inc_msg);
  }

  shutdown(sockfd, SHUT_RDWR);
}

int process_message(struct irc_message *msg) 
{

}

int connect_to_server(char *address) 
{
  struct addrinfo *addr;
  getaddr(&addr, address);

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

  /*printf("%s", buf);*/

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

  /*printf("%s", buf);*/

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

int getaddr (struct addrinfo **result, char *address) 
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
    printf("error getting addrinfo");
    exit(EXIT_FAILURE);
  }

  return 0;
}
