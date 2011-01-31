#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define   DEFAULT_PORT  "6667"
#define   BUFLENGTH   512

int getaddr (struct addrinfo **result, char *address);
int connect_to_server(char *address);

int main(int argc, char *argv[]) {

  int err, opt, aflag, cflag, nflag;
  err = opt = aflag = cflag = nflag = 0;
  char *address, *channel, *nick;

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

  int sockfd = connect_to_server(address);

  char in[BUFLENGTH];
  char out[BUFLENGTH];

  memset(out, 0, sizeof(out));
  snprintf(out, sizeof(out), "NICK %s\r\n", nick);
  send(sockfd, (void*)out, strlen(out), 0);
  memset(out, 0, sizeof(out));
  snprintf(out, sizeof(out), "USER %s 8 * : %s\r\n", nick, nick);
  send(sockfd, (void*)out, strlen(out), 0);

  int has_joined = 0;
  int bytes_rcved = 0;

  do {
    memset(in, 0, sizeof(in));
    bytes_rcved = recv(sockfd, (void*)in, sizeof(in) - 2, 0);

    char *tok = strtok(in, " ");
    if (strcmp(tok, "PING") == 0) {
      tok = strtok(NULL, "\r\n");

      memset(out, 0, sizeof(out));
      snprintf(out, sizeof(out), "PONG %s\r\n", tok);
      send(sockfd, (void*)out, strlen(out), 0);

      if (!has_joined) {
        sleep(2);
        memset(out, 0, sizeof(out));
        snprintf(out, sizeof(out), "JOIN %s\r\n", channel);
        send(sockfd, (void*)out, strlen(out), 0);
        has_joined = 1;
      }
    } else if (tok[0] == ':') {
      tok = strtok(NULL, " ");

      if (strcmp(tok, "PRIVMSG") == 0) {
        tok = strtok(NULL, " ");
        char *sender = tok;
        tok = strtok(NULL, "\r\n");
        char *message = tok;

        memset(out, 0, sizeof(out));
        snprintf(out, sizeof(out), "PRIVMSG %s :", sender);
        int offset = strlen(out);

        if (process_message(message, out+offset, sizeof(out)-offset) == 0) {
          send(sockfd, (void*)out, strlen(out), 0);
        }
      }
    }

  } while (bytes_rcved > 0);

  shutdown(sockfd, SHUT_RDWR);
}

int process_message(char *message, char *response, int response_size) {
  strncpy(response, "HELLO\r\n", response_size);
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

int getaddr (struct addrinfo **result, char *address) {
  char addr_cpy[strlen(address)];
  strcpy(addr_cpy, address);

  printf("%s %s\n", address, addr_cpy);
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
  printf("%s %s\n", name, port);


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
