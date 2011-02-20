#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bot.h"

int main(int argc, char *argv[]) 
{
  int err, opt, aflag, cflag, nflag;
  err = opt = aflag = cflag = nflag = 0;

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

  run_bot();
}
