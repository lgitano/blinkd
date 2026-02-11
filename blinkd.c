/* File: blinkd.c
   (C) 1998 W. Martin Borgert debacle@debian.org

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
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <paths.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <libintl.h>
#include <locale.h>

#include <blinkd.h>

/* macros */
#define KEYBOARDDEVICE	"/dev/console"
#define SLEEPFACTOR	100000	/* tenth of a second in micro seconds */
#define SYSLOGERR(str)	syslog (LOG_ERR, str " (line %d)\n", __LINE__)
#define SYSLOGERR1(str, arg) \
			syslog (LOG_ERR, str " (line %d)\n", arg, __LINE__)
#define SYSLOGERR2(str, arg) \
			syslog (LOG_ERR, str " (pid %d)\n", arg, getpid ())
#define LED_UNUSED      -1

/* gettext macros */
#define _(String) gettext (String)

/* type definitions */
typedef enum {CLEAR, SET, TOGGLE} ledmode_t;

/* function prototypes */
static int  create_socket     (void);
static void clear_led_on_exit (int sig_no);
static void control_led       (ledmode_t mode, int led);
static void daemon_start      (void);
static void *loop             (void *led);
static void process_opts      (int argc, char **argv);
static void threads_start     (void);
static void usage             (char *name);
static void wait_for_connect  (void);
static void wrong_use         (char *name);

/* global variables */
static int             keyboardDevice = 0;
static int             serv_tcp_port  = SERV_TCP_PORT;
static int             off_time       = 2;
static int             pause_time     = 6;
/* all three LEDs disabled */
static int             rate[3]        = { LED_UNUSED, LED_UNUSED, LED_UNUSED };
static int             on_time        = 2;
static int             leds[3]        = { LED_CAP, LED_NUM, LED_SCR };
static pthread_t       cap_thread, num_thread, scr_thread;
static pthread_mutex_t key_mutex;
static int             sockfd         = 0;
static int             noreopen       = 0;

/* main - does not return */
int
main (int argc,
      char **argv)
{
  /* gettext stuff */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  process_opts (argc, argv);
  daemon_start ();              /* start daemon */
  sockfd = create_socket ();
  threads_start ();             /* start 1..3 threads */
  if (atexit ((void (*) (void)) &clear_led_on_exit))
  {
    SYSLOGERR ("atexit() error");
  }
  signal (SIGTERM, clear_led_on_exit);
  wait_for_connect ();
  return 0;                     /* never */
}

/* clear_led_on_exit - no hanging LED after exiting blinkd, please */
static void
clear_led_on_exit (int sig_no)
{
  sig_no = sig_no;              /* get rid of compiler warning */
  if (keyboardDevice)           /* clear all LEDs and close device */
  {
    if (rate[BLINKD_CAP] != -1)
    {
      control_led (CLEAR, LED_CAP);
    }
    if (rate[BLINKD_NUM] != -1)
    {
      control_led (CLEAR, LED_NUM);
    }
    if (rate[BLINKD_SCR] != -1)
    {
      control_led (CLEAR, LED_SCR);
    }
    if (close (keyboardDevice) == -1)
    {
      SYSLOGERR ("close() %m");
    }
    keyboardDevice = 0;
  }
  if (sockfd)                   /* close socket */
  {
    close (sockfd);             /* ignore any errors */
  }
  _exit (EXIT_SUCCESS);
}

/* control_led - switch LED on or off

   This is taken from the tleds progam, written by
   Jouni.Lohikoski@iki.fi, any bugs in this routine are added by me.
*/
static void
control_led (ledmode_t mode,
             int led)
{
  char	ledVal;
  struct {
    int	led_mode;
    int	led;
  } values;
  switch (mode)
  {
    case SET:
      values.led_mode = 1;
      break;
    case CLEAR:
      values.led_mode = 0;
      break;
    case TOGGLE:
      values.led_mode = values.led_mode? 0: 1;
  }
  values.led = led;
  if (keyboardDevice && ioctl (keyboardDevice, KDGETLED, &ledVal))
  {
    SYSLOGERR ("ioctl() %m");
    if (close (keyboardDevice) == -1)
    {
      SYSLOGERR ("close() %m");
    }
    keyboardDevice = 0;
  }
  if (led == LED_CAP || led == LED_NUM || led == LED_SCR)
  {
    if (mode == SET)
    {
      ledVal |= led;
    }
    else
    {
      ledVal &= ~led;
    }
  }
  else
  {
    SYSLOGERR1 ("internal error: unknown LED: %d", led);
  }
  if (keyboardDevice && ioctl (keyboardDevice, KDSETLED, ledVal))
  {
    SYSLOGERR ("ioctl() %m");
    if (close (keyboardDevice) == -1)
    {
      SYSLOGERR ("close() %m");
    }
    keyboardDevice = 0;
  }
}

/* create_socket - create network socket for incoming tcp connection */
static int
create_socket (void)
{
  struct sockaddr_in serv_addr;

  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
  {
    SYSLOGERR ("socket() %m");
    exit (EXIT_FAILURE);
  }
  bzero ((char *) &serv_addr, sizeof (serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
  serv_addr.sin_port        = htons (serv_tcp_port);
  if (bind (sockfd, &serv_addr, sizeof (serv_addr)) == -1)
  {
    SYSLOGERR ("bind() %m");
    exit (EXIT_FAILURE);
  }
  if (listen (sockfd, 5) == -1)
  {
    SYSLOGERR ("listen() %m");
    exit (EXIT_FAILURE);
  }
  return sockfd;
}

/* daemon_start - become process group leader, disconnect from tty etc. */
static void
daemon_start (void)
{
  int fd;

  if (getppid () != 1)          /* we're not started from init(8) */
  {
    int childpid = 0;

    /* Ignore some signals. */
    signal (SIGTTOU, SIG_IGN);
    signal (SIGTTIN, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);

    /* Fork and let the parent exit to become a background process.
       This guarantees the 1st child not to be a process leader. */
    if ((childpid = fork ()) < 0)
    {
      perror ("fork");
      exit (EXIT_FAILURE);
    }
    else if (childpid > 0)
    {
      exit (EXIT_SUCCESS);      /* parent */
    }

    /* The 1st child disassociates from controlling terminal and
       process group. */
    if (setpgrp () == -1)
    {
      perror ("setpgrp");
      exit (EXIT_FAILURE);
    }
    signal (SIGHUP, SIG_IGN);
    if ((childpid = fork ()) < 0)
    {
      perror ("fork");
      exit (EXIT_FAILURE);
    }
    else if (childpid > 0)
    {
      exit (EXIT_SUCCESS);      /* parent */
    }
  }

  for (fd = 0; fd <= STDERR_FILENO; fd++)
  {
    if (close (fd) == -1)
    {
      SYSLOGERR ("close() %m");
    }
  }
  if (chdir ("/tmp") == -1)
  {
    SYSLOGERR ("chdir() %m");
  }
  umask (0);
}

/* wait_for_connect - endless loop, wait for tcp connections and update data */
static void
wait_for_connect (void)
{
  unsigned char      c = '\0';
  unsigned int       clilen, rr;
  struct sockaddr_in cli_addr;
  int                newsockfd;

  clilen = sizeof (cli_addr);
  /* The main loop */
  while (1)
  {
    if ((newsockfd = accept (sockfd, &cli_addr, &clilen)) == -1)
    {
      if (errno != EAGAIN)
      {
        SYSLOGERR ("accept() %m");
        exit (EXIT_FAILURE);
      }
    }
    else
    {
      if ((rr = read (newsockfd, &c, 1)) == 1)
      {
        int  current_led = (c >> 6) & 0x03;
        char new_rate    = c        & 0x1f;

        if ((current_led > BLINKD_ALL) || (current_led < BLINKD_CAP) ||
            (new_rate > RATE_DEC) || (new_rate < 0))
        {
          SYSLOGERR1 ("Received inappropriate blink rate 0x%0x", c);
        }
        SYSLOGERR2 ("Received appropriate blink rate 0x%0x", c);
        SYSLOGERR2 ("current_led: %d", current_led);
        SYSLOGERR2 ("new_rate: %d", new_rate);
        if (current_led != BLINKD_ALL)
        {
          if (new_rate == RATE_INC)
          {
            rate[current_led]++;
          }
          else if (new_rate == RATE_DEC)
          {
            rate[current_led]--;
          }
          else
          {
            rate[current_led] = new_rate;
          }
        }
        else                    /* resetting all LEDs */
        {
          rate[BLINKD_CAP] = (rate[BLINKD_CAP] == -1)? -1: 0;
          rate[BLINKD_NUM] = (rate[BLINKD_NUM] == -1)? -1: 0;
          rate[BLINKD_SCR] = (rate[BLINKD_SCR] == -1)? -1: 0;
        }
      }
      else if (rr == -1)
      {
        SYSLOGERR ("read() %m");
      }
      else if (rr)
      {
        SYSLOGERR1 ("read() returned %d", rr);
      }
      if (close (newsockfd) == -1)
      {
        SYSLOGERR ("close() %m");
      }
    }
  }
}

void *
loop (void *led)
{
  while (1)
  {
    if (rate [(int) led])       /* only if there is something to do */
    {
      int i;

      /* we have to open/close again and again, to follow the current
         virtual tty */
      if (!keyboardDevice)
      {
        pthread_mutex_lock (&key_mutex);
        if ((keyboardDevice = open (KEYBOARDDEVICE, O_RDONLY)) == -1)
        {
          SYSLOGERR1 ("open() on %s %m", KEYBOARDDEVICE);
          _exit (EXIT_FAILURE); /* no need to clear_led_on_exit */
        }
        pthread_mutex_unlock (&key_mutex);
      }
      for (i = 0; i < rate[(int) led]; i++)
      {
        pthread_mutex_lock (&key_mutex);
        control_led (SET, leds[(int) led]);
        pthread_mutex_unlock (&key_mutex);
        usleep (on_time * SLEEPFACTOR);
        pthread_mutex_lock (&key_mutex);
        control_led (CLEAR, leds[(int) led]);
        pthread_mutex_unlock (&key_mutex);
        usleep (off_time * SLEEPFACTOR);
      }
      usleep (pause_time * SLEEPFACTOR);
      if (keyboardDevice        /* device is open */
          && !noreopen)         /* allow closing/reopening /dev/console */
      {
        pthread_mutex_lock (&key_mutex);
        if (close (keyboardDevice) == -1)
        {
          SYSLOGERR ("close() %m");
        }
        keyboardDevice = 0;     /* reset for reopening and
                                   clear_led_on_exit */
        pthread_mutex_unlock (&key_mutex);
      }
    }
    else
    {
      sleep (1);
    }
  }
  return NULL;                  /* never reached */
}

/* process_opts - process command line, see function usage() for options */
static void
process_opts (int argc,
	      char **argv)
{
  int c           = 0;
  struct {
    unsigned int cap      : 1;
    unsigned int off_time : 1;
    unsigned int num      : 1;
    unsigned int on_time  : 1;
    unsigned int pause    : 1;
    unsigned int noreopen : 1;
    unsigned int scr      : 1;
    unsigned int tcp      : 1;
  } flags;

  memset (&flags, 0, sizeof (flags));
  while (1)
  {
    int option_index                    = 0;
    static struct option long_options[] =
    {
      {"capslockled",   0, 0, 'c'},
      {"off-time",      1, 0, 'f'},
      {"help",          0, 0, 'h'},
      {"numlockled",    0, 0, 'n'},
      {"on-time",       1, 0, 'o'},
      {"pause",         1, 0, 'p'},
      {"no-reopen",     0, 0, 'r'},
      {"scrolllockled", 0, 0, 's'},
      {"tcp-port",      1, 0, 't'},
      {"version",       0, 0, 'v'},
      {0,               0, 0, 0}
    };
    c = getopt_long (argc, argv, "cf:hno:p:rst:v",
                     long_options, &option_index);
    if (c == -1)
    {
      break;
    }
    switch (c)
    {
      case 'c':
        if (flags.cap)
        {
          wrong_use (argv[0]);
        }
        flags.cap        = 1;
        rate[BLINKD_CAP] = 0;
        break;
      case 'f':
        if (flags.off_time)
        {
          wrong_use (argv[0]);
        }
        flags.off_time = 1;
        off_time       = atoi (optarg);
        break;
      case 'h':
        usage (argv[0]);
        exit (EXIT_SUCCESS);
      case 'n':
        if (flags.num)
        {
          wrong_use (argv[0]);
        }
        flags.num        = 1;
        rate[BLINKD_NUM] = 0;
        break;
      case 'o':
        if (flags.on_time)
        {
          wrong_use (argv[0]);
        }
        flags.on_time = 1;
        on_time       = atoi (optarg);
        break;
      case 'p':
        if (flags.pause)
        {
          wrong_use (argv[0]);
        }
        flags.pause = 1;
        pause_time  = atoi (optarg);
        break;
      case 'r':
        if (flags.noreopen)
        {
          wrong_use (argv[0]);
        }
        flags.noreopen = 1;
        noreopen       = 1;
        break;
      case 's':
        if (flags.scr)
        {
          wrong_use (argv[0]);
        }
        flags.scr        = 1;
        rate[BLINKD_SCR] = 0;
        break;
      case 't':
        if (flags.tcp)
        {
          wrong_use (argv[0]);
        }
        flags.tcp      = 1;
        serv_tcp_port  = atoi (optarg);
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

  /* No LEDs specified, assuming all LEDs! */
  if (!flags.cap && !flags.num && !flags.scr)
  {
    rate[BLINKD_CAP] = 0;
    rate[BLINKD_NUM] = 0;
    rate[BLINKD_SCR] = 0;
  }
}

/* threads_start - start 1..3 threads */
static void
threads_start (void)
{

  pthread_mutex_init (&key_mutex, NULL);

  if (rate[BLINKD_CAP] != -1 &&
      pthread_create (&cap_thread,
                      NULL,
                      &loop,
                      (void *) BLINKD_CAP))
  {
    SYSLOGERR ("pthread_create");
  }
  if (rate[BLINKD_NUM] != -1 &&
      pthread_create (&num_thread,
                      NULL,
                      &loop,
                      (void *) BLINKD_NUM))
  {
    SYSLOGERR ("pthread_create");
  }
  if (rate[BLINKD_SCR] != -1 &&
      pthread_create (&scr_thread,
                      NULL,
                      &loop,
                      (void *) BLINKD_SCR))
  {
    SYSLOGERR ("pthread_create");
  }
}

/* usage - help on options */
static void
usage (char* name)
{
  printf (_("Usage: %s [options]\n"
            "Options are\n"
            "  -c,   --capslockled   use Caps-Lock LED\n"
            "  -f t, --off-time=t    set off blink time to t\n"
            "  -h,   --help          display this help and exit\n"
            "  -n,   --numlockled    use Num-Lock LED\n"
            "  -o t, --on-time=t     set on blink time to t\n"
            "  -p t, --pause=t       set pause time to t\n"
            "  -r,   --no-reopen     don't reopen /dev/console\n"
            "  -s,   --scrolllockled use Scroll-Lock LED\n"
            "  -t n, --tcp-port=n    use tcp port n\n"
            "  -v,   --version       output version information and exit\n"
            "Unit for all time values t is tenth of a second.\n"),
            name);
}

/* wrong_use - output for the user, if options cannot be interpreted */
static void
wrong_use (char *name)
{
  fprintf (stderr, _("%s: Error in arguments.  Try %s --help\n"), name, name);
  exit (EXIT_FAILURE);
}
