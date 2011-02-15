#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>
#include <dlfcn.h>

#define   DEFAULT_PORT    "6667"
#define   IRC_BUF_LENGTH  513
#define   MAX_NICK_LENGTH 500

struct irc_message 
{
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
                                   char *params);
void free_message(struct irc_message *message);
int send_nick_user();
struct irc_message *create_response(struct irc_message *msg);
int load_karma();
int save_karma();

int sockfd;
GHashTable *karma_hash;
int keep_alive;

int main(int argc, char *argv[]) 
{

  int err, opt, aflag, cflag, nflag;
  err = opt = aflag = cflag = nflag = 0;
  keep_alive = 1;

  while ((opt = getopt(argc, argv, "a:c:n:")) != -1) 
  {
    switch (opt) 
    {
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

  if (err || !aflag || !cflag || !nflag) 
  {
    fprintf(stderr, "Usage: %s -a address[:port] -c channel -n nick\n",
            argv[0]);
    fprintf(stderr, "Make sure to quote the address and channel, due to ");
    fprintf(stderr, "`:' and `#' typically\n");
    fprintf(stderr, "being special characters.\n");
    exit(EXIT_FAILURE);
  }

  load_karma();

  sockfd = connect_to_server(address);
  send_nick_user();

  int has_joined = 0;
  int bytes_rcved = 0;
  struct irc_message *inc_msg;
  struct irc_message *out_msg;

  bytes_rcved = recv_msg(&inc_msg);
  do 
  {
    if (strcmp(inc_msg->command, "PING") == 0) 
    {
      out_msg = create_message(NULL, "PONG", inc_msg->params);
      send_msg(out_msg);
      free_message(out_msg);

      if (!has_joined) 
      {
        out_msg = create_message(NULL, "JOIN", channel);
        send_msg(out_msg);
        free_message(out_msg);
        has_joined = 1;
      }
    } else 
    {
      out_msg = create_response(inc_msg);
      if (out_msg != NULL) 
      {
        send_msg(out_msg);
        free_message(out_msg);
      }
    }
    free_message(inc_msg);
    bytes_rcved = recv_msg(&inc_msg);
  } while (keep_alive && bytes_rcved > 0 && inc_msg != NULL);

  if (inc_msg != NULL) 
  {
    free_message(inc_msg);
  }

  shutdown(sockfd, SHUT_RDWR);
  save_karma();
}

int load_karma() 
{
  karma_hash = g_hash_table_new(g_str_hash, g_str_equal);
  FILE *fp = fopen("karma.txt", "r");
  char nick[MAX_NICK_LENGTH];
  int karma;
  char *n;
  int *k;
  while (fscanf(fp, "%s\t%d\n", nick, &karma) != EOF) 
  {
    n = (char*)malloc(strlen(nick)+1);
    k = (int*)malloc(sizeof(int));
    strcpy(n, nick);
    *k = karma;
    g_hash_table_insert(karma_hash, (gpointer)n, (gpointer)k);
  }
  fclose(fp);
}

int save_karma() 
{
  GList *key_list = g_hash_table_get_keys(karma_hash);
  GList *keys = key_list;
  FILE *fp = fopen("karma.txt", "w");
  char *nick;
  int *karma;
  while (keys != NULL) 
  {
    nick = (char*)keys->data;
    karma = (int*)g_hash_table_lookup(karma_hash, (gconstpointer)nick);
    fprintf(fp, "%s\t%d\n", nick, *karma);
    g_hash_table_remove(karma_hash, (gconstpointer)nick);
    free(nick);
    free(karma);
    keys = g_list_next(keys);
  }
  g_list_free(key_list);
  g_hash_table_destroy(karma_hash);
  fclose(fp);
}

struct irc_message *create_response(struct irc_message *msg) 
{
  struct irc_message *response = NULL;
  if (strcmp(msg->command, "PRIVMSG") == 0) 
  {
    char *nickname = strtok(msg->prefix, "!")+1;
    char *channel = strtok(msg->params, " "); 
    char *message = strtok(NULL, "")+1;
    char buf[IRC_BUF_LENGTH];
    char *tok = strtok(message, " ");
    if (strcmp(tok, "!QUIT") == 0 && strcmp(nickname, "brandonw") == 0) 
    {
      keep_alive = 0;
      response = create_message(NULL, "QUIT", NULL);
    } else if (strcmp(tok, "!karma") == 0) 
    {
      tok = strtok(NULL, " ");
      int* karma = (int*)g_hash_table_lookup(karma_hash, tok);
      int k = 0;
      if (karma != NULL) 
      {
        k = *karma;
      }

      sprintf(buf, "%s :%s has %d karma", channel, tok, k);
      response = create_message(NULL, "PRIVMSG", buf);
    } else if (strcmp(tok, "!up") == 0) 
    {
      tok = strtok(NULL, " ");
      int* karma = (int*)g_hash_table_lookup(karma_hash, tok);
      if (karma == NULL) 
      {
        karma = malloc(sizeof(int));
        *karma = 0;
        nick = malloc(strlen(tok)+1);
        strcpy(nick, tok);
        g_hash_table_insert(karma_hash, nick, karma);
      }
      (*karma)++;

      sprintf(buf, "%s :%s has been upvoted to %d karma",
              channel, tok, *karma);
      response = create_message(NULL, "PRIVMSG", buf);
    } else if (strcmp(tok, "!down") == 0) 
    {
      tok = strtok(NULL, " ");
      int* karma = (int*)g_hash_table_lookup(karma_hash, tok);
      if (karma == NULL) 
      {
        karma = malloc(sizeof(int));
        *karma = 0;
        nick = malloc(strlen(tok)+1);
        strcpy(nick, tok);
        g_hash_table_insert(karma_hash, nick, karma);
      }
      (*karma)--;

      sprintf(buf, "%s :%s has been downvoted to %d karma",
              channel, tok, *karma);
      response = create_message(NULL, "PRIVMSG", buf);
    }
  }
  return response;
}

int send_nick_user() 
{
  struct irc_message *nick_msg = create_message(NULL, "NICK", nick);
  send_msg(nick_msg);
  free_message(nick_msg);

  char buf[IRC_BUF_LENGTH];
  sprintf(buf, "USER %s %s 8 * : %s", nick, nick, nick);
  struct irc_message *user_msg = create_message(NULL, "USER", buf);
  send_msg(user_msg);
  free_message(user_msg);

  return 0;
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
