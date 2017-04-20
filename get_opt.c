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
#include <string.h>

#include "get_opt.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


char *get_opt(const char * const shorT,
		 const char * const lonG,
		 int argc, char **argv, int *i)
{
  // check for form long=VALUE
  size_t len = 0;
  if(*i < argc)
    {
      char *str = (char *) argv[*i];
      for(;*str && *str != '=';str++);
      if(*str == '=')
        {
          len = (size_t) str - (size_t) argv[*i];
          str++;
        }
      if(len && !strncmp(argv[*i],lonG,len)
	&&  *str != '\0')
	{
          (*i)++;
          return str;
        }
    }

  
  // check for  -a VALUE   or   --long VALUE
  if(((*i + 1) < argc) &&
     (!strcmp(argv[*i],lonG) ||
      (shorT && shorT[0] && !strcmp(argv[*i],shorT))))
    {
      (*i)++;
      return (char *) argv[(*i)++];
    }

  if(!shorT || !shorT[0]) return 0;
  // check for   -aVALUE
  len=strlen(shorT);
  if( (*i < argc) && !strncmp(argv[*i],shorT,len)
      && argv[*i][len] != '\0')
    return (char *) &(argv[(*i)++][len]);

  return 0;
}


