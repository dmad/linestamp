/* -*- mode: C; c-file-style: "gnu" -*- */
/*
 * Copyright (C) 2010 Dirk Dierckx <dirk.dierckx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#define _GNU_SOURCE
#include <getopt.h>

struct arguments
{
  char format[256];
};

static
void
print_usage (const char *command)
{
  printf ("Usage: %s [OPTION]\n"
	  "Reads stdin and sends it to stdout while prefixing each line with\n"
	  "a timestamp.\n"
	  "\n"
	  "Output:\n"
	  "  -f, --format=FORMAT\t\tformat of timestamp (see strftime)\n"
	  "\n"
	  "Miscellaneous:\n"
	  "  -V, --version\t\t\tprint version information and exit\n"
	  "  -h, --help\t\t\tdisplay this help and exit\n"
	  "\n",
	  command);
}

static
void
print_version ()
{
  printf ("%s %s\n"
          "\n"
          "Copyright 2007 by Dirk Dierckx <dirk.dierckx@dhl.com>\n"
          "This is free software; see the source for copying conditions.\n"
          "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
          "FOR A PARTICULAR PURPOSE.\n",
          PACKAGE, VERSION);
}

static
short
get_arguments (struct arguments *args, int argc, char *argv[])
{
  struct option options[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'V' },
    { "format", required_argument, NULL, 'f' },
    { NULL, 0, NULL, 0 }
  };

  int opt;
  
  memset (args, 0, sizeof (struct arguments));
  
  /* put default values */
  strcpy (args->format, "%c ");

  while (-1 != (opt = getopt_long (argc, argv, "hVf:", options, NULL))) {
    switch (opt) {
    case '?': /* invalid option */
      return 0;
    case 'h':
      print_usage (argv[0]);
      return 0;
    case 'V':
      print_version ();
      return 0;
    case 'f':
      strncpy (args->format, optarg, sizeof (args->format));
      args->format[sizeof (args->format) - 1] = '\0';
      break;
    }
  }

  return 1;
}

static 
short
write_stamp (int fd, struct arguments *args)
{
  time_t now_time;
  struct tm now_tm;
  char stamp[sizeof (args->format) * 2];
  size_t len;

  time (&now_time);
  localtime_r (&now_time, &now_tm);
  strftime (stamp, sizeof (stamp), args->format, &now_tm);
  len = strlen (stamp);
  
  if (write (fd, stamp, len) != len) {
    fprintf (stderr, "Could not write stamp to fd(%d) because: %s\n",
	     fd, strerror (errno));
    return 0;
  }

  return 1;
}

int
main (int argc, char *argv[]) 
{
  struct arguments args;

  if (get_arguments (&args, argc, argv)) {
    const size_t buffer_size = 1024 * 32;
    int ifd = STDIN_FILENO, ofd = STDOUT_FILENO;
    short busy = 1;
    char *buffer;
    short add_stamp = 1;
    
    buffer = malloc (buffer_size);
    if (NULL == buffer) {
      fprintf (stderr, "Could not allocate read buffer because: %s\n",
	       strerror (errno));
      busy = 0;
    }
    
    while (busy) {
      ssize_t read_size;
      
      read_size = read (ifd, buffer, buffer_size);
      if (-1 == read_size) {
	fprintf (stderr, "Could not read from fd(%d) because: %s\n",
		 ifd, strerror (errno));
	busy = 0;
      } else if (0 == read_size) { /* EOF */
	busy = 0;
      } else if (read_size > 0) {
	size_t processed_size = 0;
	size_t offset = 0;

	while (busy && processed_size < read_size) {
	  short write_block, new_add_stamp;

	  if ('\n' == buffer[processed_size + offset]) {
	    write_block = 1;
	    new_add_stamp = 1;
	  } else if (processed_size + offset + 1 == read_size) {
	    write_block = 1;
	    new_add_stamp = 0;
	  } else {
	    write_block = 0;
	    ++offset;
	  }

	  if (write_block) {
	    if (add_stamp) {
	      if (!write_stamp (ofd, &args))
		busy = 0; /* error occured */
	    }
	    add_stamp = new_add_stamp;
	    
	    if (busy) {
	      if (write (ofd, buffer + processed_size, offset + 1) 
		  != offset + 1) {
		fprintf (stderr, "Could not write stdin to stdout because:"
			 " %s\n", strerror (errno));
		busy = 0;
	      } else {
		processed_size += offset + 1;
		offset = 0;
	      }
	    }
	  }
	}
      }
    }

    if (NULL != buffer)
      free (buffer);
  }
  
  return 0;
}

