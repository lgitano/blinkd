/* File: blink.c
   (C) 1998 W. Martin Borgert <debacle@debian.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301, USA.
*/

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <libintl.h>
#include <locale.h>

#include <blinkd.h>

/* macros */
#define SERV_HOST "localhost"
#define RATE_MAX (RATE_INC - 1)

/* gettext macros */
#define _(String) gettext (String)

/* function prototypes */
static int  connect_server (void);
static void process_opts   (int , char **);
static void send_rate      (int);
static void usage          (char *);
static void wrong_use      (char *);
static void init_sockaddr  (struct sockaddr_in *, const char *, short);

/* global variables */
static short serv_tcp_port = SERV_TCP_PORT;
static int   led           = BLINKD_ALL;
static int   rate          = 0;
static char *server        = NULL;

/* main - boring main routine */
int
main (int argc,
      char **argv)
{
  int sockfd;

  /* gettext stuff */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  process_opts (argc, argv);
  server = (server == NULL)? SERV_HOST: server;
  sockfd = connect_server ();
  send_rate (sockfd);
  close (sockfd);
  return 0;
}

/* connect_server - talk to blinkd server */
static int
connect_server (void)
{
  int                sockfd;
  struct sockaddr_in serv_addr;

  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror ("socket");
    exit (EXIT_FAILURE);
  }
  init_sockaddr (&serv_addr, server, serv_tcp_port);
  if (connect (sockfd, &serv_addr, sizeof (serv_addr)) < 0)
  {
    perror ("connect");
    exit (EXIT_FAILURE);
  }
  return sockfd;
}

/* process_opts - process command line, see function usage() for options */
static void
process_opts (int argc,
	      char **argv)
{
  int c = 0;
  struct {
    unsigned int led      : 1;
    unsigned int machine  : 1;
    unsigned int rate     : 1;
    unsigned int tcp_port : 1;
  } flags;

  memset (&flags, 0, sizeof (flags));
  while (1)
  {
    int option_index                    = 0;
    static struct option long_options[] =
    {
      {"capslockled",   0, 0, 'c'},
      {"help",          0, 0, 'h'},
      {"machine",       1, 0, 'm'},
      {"numlockled",    0, 0, 'n'},
      {"rate",          1, 0, 'r'},
      {"scrolllockled", 0, 0, 's'},
      {"tcp-port",      1, 0, 't'},
      {"version",       0, 0, 'v'},
      {0,               0, 0, 0}
    };
    c = getopt_long (argc, argv, "chm:nr:st:v", long_options, &option_index);
    if (c == -1)
    {
      break;
    }
    switch (c)
    {
      case 'c':
        if (flags.led)
        {
          wrong_use (argv[0]);
        }
        led       = BLINKD_CAP;
        flags.led = 1;
        break;
      case 'h':
        usage (argv[0]);
        exit (EXIT_SUCCESS);
      case 'm':
        if (flags.machine)
        {
          wrong_use (argv[0]);
        }
        flags.machine = 1;
        server        = optarg;
        break;
      case 'n':
        if (flags.led)
        {
          wrong_use (argv[0]);
        }
        led       = BLINKD_NUM;
        flags.led = 1;
        break;
      case 'r':
        if (flags.rate)
        {
          wrong_use (argv[0]);
        }
        flags.rate = 1;
        if (!strcmp ("+", optarg))
        {
          rate = RATE_INC;
        }
        else if (!strcmp ("-", optarg))
        {
          rate = RATE_DEC;
        }
        else
        {
          rate = atoi (optarg);
          if (rate < 0 || rate > RATE_MAX)
          {
            fprintf (stderr,
                     _("Error.  Use value from 0 to %d for --rate.\n"),
                     RATE_MAX);
            rate = (rate)? RATE_MAX: 0;
          }
        }
        break;
      case 's':
        if (flags.led)
        {
          wrong_use (argv[0]);
        }
        led       = BLINKD_SCR;
        flags.led = 1;
        break;
      case 't':
        if (flags.tcp_port)
        {
          wrong_use (argv[0]);
        }
        flags.tcp_port      = 1;
        serv_tcp_port = atoi (optarg);
        break;
      case 'v':
        puts (PACKAGE " " VERSION);
        exit (EXIT_SUCCESS);
        break;
      default:
        wrong_use (argv[0]);
    }
  }
  if (optind < argc)
  {
    wrong_use (argv[0]);
  }
  /* A blink rate <> 0 is only useful when specifying an LED */
  if (flags.rate && (rate != 0) && !flags.led)
  {
    wrong_use (argv[0]);
  }
}

/* send_rate - send new blink rate to the server */
static void
send_rate (int sockfd)
{
  unsigned char octet;

  octet = (led << 6);
  octet |= rate;
  if (write (sockfd, (const void *) &octet, 1) != 1)
  {
    perror ("write");
    exit (EXIT_FAILURE);
  }
}

/* usage - help on options */
static void
usage (char* name)
{
  printf (_("Usage: %s [options]\n"
            "Options are\n"
            "  -c,   --capslockled   use Caps-Lock LED\n"
            "  -h,   --help          display this help and exit\n"
            "  -m s, --machine=s     let keyboard of machine s blink\n"
            "  -n,   --numlockled    use Num-Lock LED\n"
            "  -r n, --rate=n        set blink rate to n\n"
            "  -s,   --scrolllockled use Scroll-Lock LED\n"
            "  -t n, --tcp-port=n    use tcp port n\n"
            "  -v,   --version       output version information and exit\n"),
          name);
}

/* wrong_use - output for the user, if options cannot be interpreted */
static void
wrong_use (char *name)
{
  fprintf (stderr, _("%s: Error in arguments.  Try %s --help.\n"), name, name);
  exit (EXIT_FAILURE);
}

/* init_sockaddr - initialize socket struct with hostname and port number */
static void
init_sockaddr (struct sockaddr_in *name,
               const char *hostname,
               short port)
{
  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, _("Unknown host %s.\n"), hostname);
    exit (EXIT_FAILURE);
  }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}
