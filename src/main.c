#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>

#define   DEFAULT_PORT    "6667"
#define   IRC_BUF_LENGTH  513

struct irc_message {
  char prefix[IRC_BUF_LENGTH];
  char command[IRC_BUF_LENGTH];
  char params[IRC_BUF_LENGTH];
};

char *address, *channel, *nick;

int getaddr (struct addrinfo **result, char *address);
int connect_to_server(char *address);
int send_msg(struct irc_message *message);
int recv_msg(struct irc_message **message);
struct irc_message *create_message(char *prefix, 
                                   char *command, 
                                   int nparams,
                                   ...);
void free_message(struct irc_message *message);
int send_nick_user();

int sockfd;

int main(int argc, char *argv[]) {

  int err, opt, aflag, cflag, nflag;
  err = opt = aflag = cflag = nflag = 0;

  while ((opt = getopt(argc, argv, "a:c:n:")) != -1) {
    switch (opt) {
      case 'a':
        address = optarg;
        aflag = 1;
        break;
      case 'c':
        channel = optarg;
        cflag = 1;
        break;
      case 'n':
        nick = optarg;
        nflag = 1;
        break;
      default:
        err = 1;
        break;
    }
  }

  if (err || !aflag || !cflag || !nflag) {
    fprintf(stderr, "Usage: %s -a address[:port] -c channel -n nick\n",
            argv[0]);
    fprintf(stderr, "Make sure to quote the address and channel, due to :\n");
    fprintf(stderr, "and # typically being special characters.\n");
    exit(EXIT_FAILURE);
  }

  sockfd = connect_to_server(address);
  send_nick_user();

  int has_joined = 0;
  int bytes_rcved = 0;
  struct irc_message *inc_msg;
  struct irc_message *out_msg;

  bytes_rcved = recv_msg(&inc_msg);
  do {
    if (strcmp(inc_msg->command, "PING") == 0) {
      out_msg = create_message(NULL, "PONG", 1, inc_msg->params);
      send_msg(out_msg);
      free_message(out_msg);

      if (!has_joined) {
        out_msg = create_message(NULL, "JOIN", 1, channel);
        send_msg(out_msg);
        free_message(out_msg);
        has_joined = 1;
      }
    } else if (strcmp(inc_msg->command, "PRIVMSG") == 0) {

    } else if (strcmp(inc_msg->command, "JOIN") == 0 &&
               strcmp(inc_msg->params, ":#triangle") == 0) {
      char *nickname = strtok(inc_msg->prefix, "!");
      out_msg = create_message(NULL, "MODE", 3, "#triangle", "+O", nickname);
      send_msg(out_msg);
      free_message(out_msg);
    }

    free_message(inc_msg);
    bytes_rcved = recv_msg(&inc_msg);
  } while (bytes_rcved > 0 && inc_msg != NULL);

  shutdown(sockfd, SHUT_RDWR);
}

int send_nick_user() {
  struct irc_message *nick_msg = create_message(NULL, "NICK", 1, nick);
  send_msg(nick_msg);
  free_message(nick_msg);

  struct irc_message *user_msg = create_message(NULL, "USER", 6, nick, nick,
                                                "8", "*", ":", nick);
  send_msg(user_msg);
  free_message(user_msg);

  return 0;
}

int connect_to_server(char *address) {
  struct addrinfo *addr;
  getaddr(&addr, address);

  int sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (sockfd < 0) {
    perror("socket");
    exit (EXIT_FAILURE);
  }

  int conn_result;
  conn_result = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
  if (conn_result < 0) {
    printf("Failure connecting to %s", address);
    return -1;
  }
  freeaddrinfo(addr);

  return sockfd;
}

int send_msg(struct irc_message *message) {
  static char buf[IRC_BUF_LENGTH];
  memset(buf, 0, sizeof(buf));
  int idx = 0;

  if (message->prefix[0] != '\0') {
    sprintf(buf+idx, "%s ", message->prefix);
    idx += strlen(message->prefix) + 1;
  }

  sprintf(buf+idx, "%s", message->command);
  idx += strlen(message->command);

  if (message->params[0] != '\0') {
    sprintf(buf+idx, " %s", message->params);
    idx += strlen(message->params) + 1;
  }

  sprintf(buf+idx, "\r\n");

  return send(sockfd, (void*)buf, strlen(buf), 0);
}

int recv_msg(struct irc_message **message) {
  static char buf[IRC_BUF_LENGTH];
  int bytes_rcved = 0;
  memset(buf, 0, sizeof(buf));

  do
    bytes_rcved += recv(sockfd, (void*)(buf + bytes_rcved), 1, 0);
   while (buf[bytes_rcved - 1] != '\n');
  
  if (bytes_rcved <= 0)
    return bytes_rcved;

  char *prefix, *command, *params;
  prefix = command = params = NULL;

  char *tok = strtok(buf, " ");
  if (tok[0] != ':') {
    command = tok;
  }
  else {
    prefix = tok;
    command = strtok(NULL, " ");
  }

  if ((tok = strtok(NULL, "\r\n")) != NULL) {
    params = tok;
  }
  
  *message = create_message(prefix, command, 1, params);
  return bytes_rcved;
}

struct irc_message *create_message(char *prefix, 
                                   char *command, 
                                   int nparams,
                                   ...) {

  struct irc_message *msg = malloc(sizeof(struct irc_message));
  memset(msg->prefix, 0, sizeof(msg->prefix));
  memset(msg->command, 0, sizeof(msg->command));
  memset(msg->params, 0, sizeof(msg->params));

  int len = 0;
  if (prefix != NULL) {
    len += strlen(prefix) + 1; /* +1 for space after prefix */
    if (len > IRC_BUF_LENGTH) {
      free_message(msg);
      return NULL;
    }
    strcpy(msg->prefix, prefix);
  }

  len += strlen(command);
  if (len > IRC_BUF_LENGTH) {
    free_message(msg);
    return NULL;
  }
  strcpy(msg->command, command);

  if (nparams > 0) {
    va_list ap;
    va_start(ap, nparams);
    char *param;
    int param_idx = 0;
    int is_first_param = 1;

    while (nparams > 0) {
      param = va_arg(ap, char *);
      len += strlen(param) + 1;
      if (len > IRC_BUF_LENGTH) {
        free_message(msg);
        va_end(ap);
        return NULL;
      }
      if (!is_first_param) {
        strcpy(msg->params + param_idx, " ");
        param_idx++;
      }
      strcpy(msg->params + param_idx, param);
      param_idx += strlen(param);
      nparams--;
      is_first_param = 0;
    }
    va_end(ap);
  }

  return msg;
}

void free_message(struct irc_message *message) {
  free(message);
  return;
}

int getaddr (struct addrinfo **result, char *address) {
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
  if (s != 0) {
    printf("error getting addrinfo");
    exit(EXIT_FAILURE);
  }

  return 0;
}
