/*
  Quickplot - an interactive 2D plotter

  Copyright (C) 1998-2011  Lance Arsenault


  This file is part of Quickplot.

  Quickplot is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Quickplot is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Quickplot.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "paths.h"



#include "quickplot.h"
#include "config.h"
#include "debug.h"
#include "list.h"
#include "channel.h"
#include "spew.h"
#include "channel_double.h"
#include "qp.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static int LaunchBrowser(const char * const url)
{
  int i=0;
  pid_t pid;
  const char *browser;
  
  // now try the list of browsers
  const char * const browsers[] =
  {
    "firefox",
    "chromium-browser",
    "galeon",
    "konqueror",
    "mozilla",
    "epiphany",
    "opera",
    "netscape",
    "dillo",
    "seamonkey",
    /*
    "amaya",
    "browsex",
    "light",
    */
    NULL
  };

 
  /* Lynx is so cool I've got to try it */
  //execlp("lynx", "lynx", url, NULL);
  /* It worked. That was cool. */


  pid = vfork();
  if(pid < 0) //error
  {
    QP_WARN("vfork() failed");
    return 1;
  }

  if(pid > 0) // parent
    return 0;

  // I'm the child.

  // First see if the user has set a prefered browser in the BROWSER
  // environment variable.
  browser = getenv("BROWSER");

  if(browser)
    execlp(browser, browser, url, NULL);

  browser =  getenv("QP_BROWSER");

  if(browser)
    execlp(browser, browser, url, NULL);


  for(browser=browsers[i]; browser; browser=browsers[++i])
    {
      INFO("executing: %s %s\n", browser, url);
      execlp(browser, browser, url, NULL);
    }

  QP_WARN("Failed to launch a browser");

  exit(EXIT_FAILURE); // child exits
}

/* for finding the full path to
 * help.txt about.txt about.html help.html index.html
 * Returns an open fd or -1 on failure 
 * Returns fullpath from malloc() */
int qp_find_doc_file(const char *filename, char **fullpath_ret)
{
  int i;
  char *fullpath;
  char *htmldir[4];

  ASSERT(strlen(filename) > 5);

  htmldir[0] = getenv("QUICKPLOT_HTMLDIR");
  htmldir[1] = getenv("QUICKPLOT_DOCDIR");
  htmldir[2] = HTMLDIR;
  htmldir[3] = DOCDIR;

  for(i=0;i<4;++i)
  {
    int fd;

    if(!htmldir[i]) continue;

    fullpath = qp_malloc(strlen(filename) + 2 + strlen(htmldir[i]));
    sprintf(fullpath, "%s%c%s", htmldir[i], DIR_CHAR, filename);
    
    fd = open(fullpath, O_RDONLY);
    if(fd == -1)
    {
      QP_INFO("Can't open file \"%s\"\n", fullpath);
      free(fullpath);
      continue;
    }
    else
    {
      if(fullpath_ret)
        *fullpath_ret = fullpath;
      return fd;
    }
  }

  if(fullpath_ret)
    *fullpath_ret = NULL;

  {
    int is_txt;
    is_txt = strcmp(".txt", &filename[strlen(filename)-4])?0:1;

    QP_WARN("Can't open Quickplot documentation file \"%s\"\n"
        "Try setting environment variable QUICKPLOT_%s to\n"
        "the directory where this file was installed to.\n",
        filename, is_txt?"DOCDIR":"HTMLDIR");
  }
  return -1; /* failed */ 
}


int qp_launch_browser(const char *fileName)
// Returns 0 on success
{
  int ret = 1;
  int fd;
  char *fullpath = NULL;

  ASSERT(fileName);
  ASSERT(fileName[0]);
  ASSERT(fileName[0] != DIR_CHAR);

  fd = qp_find_doc_file(fileName, &fullpath);

  if(fullpath)
  {
    close(fd);
    ret = LaunchBrowser(fullpath);
    free(fullpath);
  }
  return ret;
}

