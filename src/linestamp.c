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

#include "arguments.h"

struct arguments
{
  char format[256];
};

static
void
_print_usage_header (const char *command)
{
  printf ("Usage: %s [OPTION]\n"
	  "Reads stdin and sends it to stdout while prefixing each line with\n"
	  "a timestamp.\n", 
	  command);
}

static
void
_print_version ()
{
  printf ("%s %s\n"
          "\n"
          "Copyright 2010 by Dirk Dierckx <dirk.dierckx@gmail.com>\n"
          "This is free software; see the source for copying conditions.\n"
          "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
          "FOR A PARTICULAR PURPOSE.\n",
          PACKAGE, VERSION);
}

static
bool
_process_option (struct arguments_definition *def,
		 int opt,
		 const char *optarg,
		 int argc,
		 char *argv[])
{
  struct arguments *args = def->user_data;
  bool go_on = true;

  switch (opt) {
  case '?': /* invalid option */
    go_on = false;
    break;
  case ':': /* invalid argument */
  case 'h':
    print_usage (def, argv[0]);
    go_on = false;
    break;
  case 'V':
    _print_version ();
    go_on = false;
    break;
  case 'f':
    strncpy (args->format, optarg, sizeof (args->format));
    args->format[sizeof (args->format) - 1] = '\0';
    break;
  }

  return go_on;
}

static
bool
_get_arguments (struct arguments *args, int argc, char *argv[])
{
  struct arguments_definition def;
  struct arguments_option options[] = {
    { "Output", 'f', "format", required_argument, "FORMAT",
      "format of timestamp (see strftime)" },
    { "Miscellaneous", 'V', "version", no_argument, NULL,
      "print version information and exit" },
    { "Miscellaneous", 'h', "help", no_argument, NULL,
      "display this help and exit" },
    { NULL, 0, NULL, 0, NULL, NULL }
  };
  
  memset (&def, 0, sizeof (def));

  def.print_usage_header = &_print_usage_header;
  def.process_option = &_process_option;
  def.process_non_options = NULL;
  def.options = options;
  
  memset (args, 0, sizeof (struct arguments));

  strcpy (args->format, "%c ");

  def.user_data = args;

  return get_arguments (&def, argc, argv);
}

static 
bool
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
    return false;
  }

  return true;
}

int
main (int argc, char *argv[]) 
{
  struct arguments args;

  if (_get_arguments (&args, argc, argv)) {
    const size_t buffer_size = 1024 * 32;
    int ifd = STDIN_FILENO, ofd = STDOUT_FILENO;
    bool busy = true, add_stamp = true;
    char *buffer;
    
    buffer = malloc (buffer_size);
    if (NULL == buffer) {
      fprintf (stderr, "Could not allocate read buffer because: %s\n",
	       strerror (errno));
      busy = false;
    }
    
    while (busy) {
      ssize_t read_size;
      
      read_size = read (ifd, buffer, buffer_size);
      if (-1 == read_size) {
	fprintf (stderr, "Could not read from fd(%d) because: %s\n",
		 ifd, strerror (errno));
	busy = false;
      } else if (0 == read_size) { /* EOF */
	busy = false;
      } else if (read_size > 0) {
	size_t processed_size = 0;
	size_t offset = 0;

	while (busy && processed_size < read_size) {
	  bool write_block, new_add_stamp;

	  if ('\n' == buffer[processed_size + offset]) {
	    write_block = true;
	    new_add_stamp = true;
	  } else if (processed_size + offset + 1 == read_size) {
	    write_block = true;
	    new_add_stamp = false;
	  } else {
	    write_block = false;
	    ++offset;
	  }

	  if (write_block) {
	    if (add_stamp) {
	      if (!write_stamp (ofd, &args))
		busy = false; /* error occured */
	    }
	    add_stamp = new_add_stamp;
	    
	    if (busy) {
	      if (write (ofd, buffer + processed_size, offset + 1) 
		  != offset + 1) {
		fprintf (stderr, "Could not write stdin to stdout because:"
			 " %s\n", strerror (errno));
		busy = false;
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
