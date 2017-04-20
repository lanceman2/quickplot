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
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <inttypes.h>

#include <sndfile.h>
#include <gtk/gtk.h>

#include "quickplot.h"

#include "config.h"
#include "qp.h"
#include "debug.h"
#include "spew.h"
#include "term_color.h"
#include "list.h"
#include "channel.h"
#include "channel_double.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif



#define BUF_LEN  (4096)

static __thread ssize_t (*sys_read)(int fd,
      void *buf, size_t count) = NULL;

static __thread off_t (*sys_lseek)(int fd,
    off_t offset, int whence) = NULL;

static __thread struct qp_reader *qp_rd = NULL;

struct qp_reader
{
  int fd;
  FILE *file;
  /* We use this buffer to virtualize read()
   * We think that libsndfile will know that
   * the file good by reading just RD_LEN. */
  uint8_t *buf;
  size_t len, /* bytes read from system read() */
         rd;  /* bytes read by reader */
  int past; /* read past the buffer */
  char *filename;
};


/* Wrapping the system read() just so we can read
 * a pipe with the libsndfile API and if the
 * pipe was not sound data then we can read
 * what was in the front of the pipe from 
 * the qp_rd buffer.  It turns out that stdio
 * streams are in the same library as the read()
 * function and do not call my wrapper.
 * So we use our own Getline() that starts by
 * copying from the qp_rd buffer and than switches
 * to calling getline() after the buffer is all
 * copied.  This function does more than we
 * need, because we wrongly thought that getline would
 * call it.  We are lucky that libsndfile does not
 * use stdio streams to read. */
ssize_t read(int fd, void *buf, size_t count)
{
  if(!sys_read)
  {
    char *error;
    dlerror();
    sys_read = dlsym(RTLD_NEXT, "read");
    if((error = dlerror()) != NULL)
    {
      EERROR("dlsym(RTLD_NEXT, \"read\") failed: %s\n", error);
      QP_EERROR("Failed to virtualize read(): %s\n", error);
      exit(1);
    }
  }


  if(qp_rd && qp_rd->fd == fd && !qp_rd->past)
  {
    ssize_t n;
    size_t rcount;

    //DEBUG("count=%zu  rd=%zu len=%zu\n", count, qp_rd->rd, qp_rd->len);

    if(BUF_LEN == qp_rd->rd)
    {
      /* The read before this one was the last
       * read that was buffered.  Now it is reading
       * past the buffer and we are no longer
       * buffering the read.  We are screwed if
       * we need more buffering. */
      qp_rd->past = 1;
      DEBUG("Finished virtualizing read()\n");
      return sys_read(fd, buf, count);
    }

    if(qp_rd->len >= qp_rd->rd + count)
    {
      /* pure virtual read */
      memcpy(buf, &qp_rd->buf[qp_rd->rd], count);
      qp_rd->rd += count;
      return (ssize_t) count;
    }

    if(BUF_LEN == qp_rd->len)
    {
      /* We cannot read any more and
       * we still have unread buffer data.
       * pure virtual read with less than
       * requested returned */
      /* count is greater than what is buffered */
      n = BUF_LEN - qp_rd->rd;
      memcpy(buf, &qp_rd->buf[qp_rd->rd], n);
      qp_rd->rd = BUF_LEN;
      return n;
    }

    /* At this point we will read more into
     * the buffer */

    if(count > BUF_LEN - qp_rd->rd)
      /* Giving less than asked for.
       * We may fill the buffer full. */
      rcount = BUF_LEN - qp_rd->len;
    else
      /* Try to give what was asked for. */
      rcount = count + qp_rd->rd - qp_rd->len;

    errno = 0;
    n = sys_read(fd, &qp_rd->buf[qp_rd->rd], rcount);

    if(n < 0)
    {
      EWARN("read(fd=%d, buf=%p, count=%zu) failed\n",
          fd, &qp_rd->buf[qp_rd->rd], rcount);
      QP_EWARN("reading file \"%s\" failed", qp_rd->filename);
      qp_rd->past = 1;
      return n;
    }
    if(n == 0 && qp_rd->rd == qp_rd->len)
    {
      /* virtual and real end of file */
      //DEBUG("read(fd=%d, buf=%p, count=%zu)=0 end of file\n",
      //    fd, &qp_rd->buf[qp_rd->rd], rcount);
      return n;
    }
   
    /* assuming that the OS is not a piece of shit */
    ASSERT(n <= rcount);

    qp_rd->len += n;
    n = qp_rd->len - qp_rd->rd;
    memcpy(buf, &qp_rd->buf[qp_rd->rd], n);
    qp_rd->rd += n;
    return n;
  }
  else
    return sys_read(fd, buf, count);
}


/* We virtualize lseek() to work around a bug in libsndfile 1.0.25:
 * it calls lseek() when reading a pipe with Ogg file format data. */
off_t lseek(int fd, off_t offset, int whence)
{
  if(!sys_lseek)
  {
    char *error;
    dlerror();
    sys_lseek = dlsym(RTLD_NEXT, "lseek");
    if((error = dlerror()) != NULL)
    {
      EERROR("dlsym(RTLD_NEXT, \"lseek\") failed: %s\n", error);
      QP_EERROR("Failed to virtualize lseek(): %s\n", error);
      ASSERT(0);
      exit(1);
    }
  }

  if(qp_rd && qp_rd->fd == fd && !qp_rd->past && whence == SEEK_SET)
  {
    if(offset > BUF_LEN || offset > qp_rd->rd)
    {
      ERROR("Virtualized lseek(fd=%d, offset=%ld, SEEK_SET) failed\n",
          fd, offset);
      QP_ERROR("Failed to virtualize lseek(fd=%d, offset=%ld, SEEK_SET)"
          " values where not expected.\n", fd, offset);
      ASSERT(0);
      exit(1);
    }

    DEBUG("Virtualizing lseek(fd=%d, offset=%ld, SEEK_SET)\n", fd, offset);

    qp_rd->rd = offset;
    return offset; /* success */
  }

  return sys_lseek(fd, offset, whence);
}

/* This starts by copying data from the qp_rd
 * read() buffer and then when that is empty it
 * finishes reading the file to complete a line,
 * and after that it just calls getline() */
static inline
ssize_t Getline(char **lineptr, size_t *n, FILE *file)
{
  ssize_t i;
  int c;

  if(!qp_rd || qp_rd->rd == qp_rd->len)
    return getline(lineptr, n, file);
  
  i = qp_rd->rd;


  while(i < qp_rd->len && qp_rd->buf[i] != '\n')
    ++i;

  if(qp_rd->buf[i] == '\n')
  {
    i -= qp_rd->rd;
    ++i; /* copy the '\n' too */
    if(*n < i + 1)
      *lineptr = qp_realloc(*lineptr, i + 1);
    memcpy(*lineptr, &qp_rd->buf[qp_rd->rd], i);
    qp_rd->rd += i;
    (*lineptr)[i] = '\0';
    *n = i + 1;
    return i;
  }
  
  /* i == qp_rd->len  && qp_rd->buf[i] != '\n' */

  i -= qp_rd->rd; /* copy i bytes from the qp_rd buffer */

  if(*n < i + 1)
  {
    *n = i + 1 + 128;
    *lineptr = qp_realloc(*lineptr, *n);
  }
  memcpy(*lineptr, &qp_rd->buf[qp_rd->rd], i);
  qp_rd->rd = qp_rd->len;
  
  /* Now we need to read the file until '\n' or EOF */

  c = getc(file);
  while(c != EOF)
  {
    (*lineptr)[i++] = c;
    /* we now have i chars in *lineptr */
    if(*n == i + 1)
      *lineptr = qp_realloc(*lineptr , (*n += 128));
    if(c == '\n')
      break;
    c = getc(file);
  }
  (*lineptr)[i] = '\0';

  return i;
}


static
int read_ascii(qp_source_t source, struct qp_reader *rd)
{
  char *line = NULL;
  size_t line_buf_len = 0;
  ssize_t n;
  size_t line_count = 0;
  int (*parse_line)(struct qp_source *source,
      char *line);
  int data_flag = -1;

  /* TODO: check for other types */
  source->value_type = QP_TYPE_DOUBLE;

  switch(source->value_type)
  {
    /* TODO: brake these cases up into
     * other data parsers. */
    case QP_TYPE_DOUBLE:

      parse_line = qp_source_parse_doubles;
      break;

  /* TODO: add other cases */

    default:
      ASSERT(0);
      break;
  }

  if(app->op_skip_lines)
  {
    size_t skip_lines;
    skip_lines = app->op_skip_lines;

    while(skip_lines--)
    {
      errno = 0;
      n = Getline(&line, &line_buf_len, rd->file);

      ++line_count;

      if(n == -1 || errno)
      {
        if(!errno)
        {
          /* end-of-file so it is zero length */
          WARN("getline() read no data in file %s\n",
            source->name);
          QP_WARN("read no data in file %s\n",
            source->name);
        }
        else
        {
          EWARN("getline() read no data in file %s\n",
            source->name);
          QP_EWARN("read no data in file %s\n",
            source->name);
        }

        if(line)
          free(line);
        return 1; /* error */
      }
    }
  }

#define CHUNK 16

  if(app->op_labels)
  {
    char *s, *sep;
    size_t sep_len, num_labels = 0, mem_len = CHUNK;

    source->labels = qp_malloc(sizeof(char *)*(mem_len+1));

    errno = 0;
    n = Getline(&line, &line_buf_len, rd->file);

    ++line_count;

    if(n == -1 || errno)
    {
      if(!errno)
      {
        /* end-of-file so it is zero length */
        WARN("getline() read no data in file %s\n",
          source->name);
        QP_WARN("read no data in file %s\n",
          source->name);
      }
      else
      {
        EWARN("getline() read no data in file %s\n",
          source->name);
        QP_EWARN("read no data in file %s\n",
          source->name);
      }

      if(line)
          free(line);
        return 1; /* error */
    }

    s = line;
    sep = app->op_label_separator;
    sep_len = strlen(sep);
    
    do
    {
      char *end;
      size_t len;
      end = s;
      /* find the next separator */
      end = s;
      while(*end && *end != '\n' && *end != '\r' &&
          strncmp(end, sep, sep_len))
        ++end;

      /* *end == '\0' or end == "the separator" */
      len = end - s;
      /* get a label */
      source->labels[num_labels] = qp_strndup(s, len);
      ++num_labels;

      if(!(*end) || *end == '\n' || *end == '\r')
        break;

      /* skip the separator */
      s = end + sep_len;
      if(!(*s))
        break;

      if(num_labels == mem_len)
      {
        mem_len += CHUNK;
        source->labels = qp_realloc(source->labels,
            sizeof(char *)*(mem_len+1));
      }

    } while(*s);

    source->labels[num_labels] = NULL;
    source->num_labels = num_labels;
  }
#undef CHUNK

  do
  {
    /* we seperate the first read because it shows
     * the error case when the file has no data at all. */
    errno = 0;
    n = Getline(&line, &line_buf_len, rd->file);

    ++line_count;
    if(n == -1 || errno)
    {
      if(!errno)
      {
        /* end-of-file so it is zero length */
        WARN("getline() read no data in file %s\n",
            source->name);
        QP_WARN("read no data in file %s\n",
            source->name);
      }
      else
      {
        EWARN("getline() read no data in file %s\n",
            source->name);
        QP_EWARN("read no data in file %s\n",
            source->name);

      }
      if(line)
        free(line);
      return 1; /* error */
    }
    data_flag = parse_line(source, line);
  } while(data_flag == 0);

  /* Now we have an least one line of values.
   * We would have returned if we did not. */

  errno = 0;
  while((n = Getline(&line, &line_buf_len, rd->file)) > 0)
  {
    ++line_count;
    parse_line(source, line);
    errno = 0;
  }

  if(line)
    free(line);

  /* n == -1 and errno == 0 on end-of-file */
  if((n == -1 && errno != 0) || errno)
  {
#if QP_DEBUG
    if(errno)
      EWARN("getline() failed to read file %s\n",
          source->name);
    else
      WARN("getline() failed to read file %s\n",
          source->name);
#endif
    QP_WARN("failed to read file %s\n", source->name);
    return 1; /* error */
  }

  return 0; /* success */
}

/* returns an allocated string */
static char *unique_name(const char *name)
{
  size_t num = 1, len = 0;
  char *buf = NULL;
  ASSERT(name && name[0]);
  ASSERT(name[strlen(name)-1] != DIR_CHAR);

  if(name[0] == '-' && !name[1])
  {
    name = "stdin";
    /* Sometimes the user wants to know why the
     * program is just hanging.  It is hanging
     * because it is waiting for input from the
     * terminal? */
    QP_NOTICE("Reading stdin\n");
  }


  {
    /* we use just the base name without the directory part */
    char *s;
    s = (char *) &name[strlen(name)-1];
    while(s != name && *s != DIR_CHAR)
      --s;
    if(*s == DIR_CHAR)
      ++s;
    buf = (char *) (name = qp_strdup(s));
  }

  while(1)
  {
    struct qp_source *s;
    for(s=(struct qp_source*) qp_sllist_begin(app->sources);
        s;
        s=(struct qp_source*) qp_sllist_next(app->sources))
      if(strcmp(s->name, buf) == 0)
        break;
    if(s)
    {
      ++num;
      if(buf == name)
        buf = (char*) qp_malloc(len= strlen(buf)+16);
      snprintf(buf, len, "%s(%zu)", name, num);
    }
    else
    {
      if(buf != name)
        free((char*)name);
      return buf;
    }
  }
}


static inline
struct qp_source *
make_source(const char *filename, int value_type)
{
  qp_app_check();
  ASSERT(filename && filename[0]);
  struct qp_source *source;
  source = (struct qp_source *)
    qp_malloc(sizeof(struct qp_source));
  source->name = unique_name(filename);
  source->num_values = 0;
  source->value_type = (value_type)?value_type:QP_TYPE_DOUBLE;
  source->num_channels = 0;
  source->labels = NULL;
  source->num_labels = 0;
  /* NULL terminated array on channels */
  source->channels = qp_malloc(sizeof(struct qp_channel *));
  *(source->channels) = NULL;
  qp_sllist_append(app->sources, source);

  return source;
}

/* Returns 0 if the file is read as a libsndfile
 * Returns 1 is not.
 * Returns -1 and spews if we have a system read error */
static
int read_sndfile(struct qp_source *source, struct qp_reader *rd)
{
  double *x, rate;
  size_t count;
  SNDFILE *file;
  SF_INFO info;
  size_t skip_lines;

  skip_lines = app->op_skip_lines;

  file = sf_open_fd(rd->fd, SFM_READ, &info, 0);
  if(!file)
  {
    QP_INFO("file \"%s\" is not readable by libsndfile\n",
        rd->filename);
    return 1; /* not a libsndfile */
  }
  else if(info.frames < skip_lines)
  {
    QP_INFO("file \"%s\" is readable by libsndfile with %"PRId64
        " samples which is less the lines to skip from --skip-lines=%zu\n",
        rd->filename, info.frames, skip_lines);
    return -1; /* error */
  }


  rate = info.samplerate;
  x = qp_malloc(sizeof(double)*(info.channels+1));

  source->num_channels = info.channels+1;
  source->channels = qp_realloc(source->channels,
        sizeof(struct qp_channel *)*(info.channels+2));
  source->channels[source->num_channels] = NULL;
  source->value_type = QP_TYPE_DOUBLE;

  /* use count as dummy index for now */
  for(count=0; count<source->num_channels; ++count)
    source->channels[count] =
      qp_channel_create(QP_CHANNEL_FORM_SERIES, QP_TYPE_DOUBLE);

  count = 0;

  while(1)
  {
    int i;

    if(sf_readf_double(file, x, 1) < 1)
      break;

    if(skip_lines)
    {
      --skip_lines;
      continue;
    }

    /* TODO: use other channel types like load as shorts or ints */

    qp_channel_series_double_append(source->channels[0], count/rate);
    for(i=0;i<info.channels;++i)
       qp_channel_series_double_append(source->channels[i+1], x[i]);

    ++count;
  }

  source->num_values = count;
  free(x);
  sf_close(file);

  if(count)
  {
    char label0[128];
    snprintf(label0, 128, "time (1/%d sec)", info.samplerate);
  
    source->labels = qp_malloc(sizeof(char *)*2);
    source->labels[0] = qp_strdup(label0);
    source->labels[1] = NULL;
    source->num_labels = 1;

    DEBUG("read\nlibsndfile \"%s\" with %d sound channel(s), "
        "at rate %d Hz with %zu values, %g seconds of sound\n",
        rd->filename,
        info.channels, info.samplerate,
        count, count/rate);
    return 0; /* success */
  }

  QP_WARN("No sound data found in file \"%s\"\n", rd->filename);
  return -1; /* fail no data in file, caller cleans up */
}

/* returns non-zero if fd is a pipe */
static int is_pipe(struct qp_reader *rd)
{
  struct stat st;
  if(fstat(rd->fd, &st) == -1)
  {
    QP_ERROR("fstat() failed to stat file \"%s\"\n", rd->filename);
    exit(1);
  }
  return (st.st_mode & S_IFIFO);
}


qp_source_t qp_source_create(const char *filename, int value_type)
{
  struct qp_source *source;
  struct qp_reader rd;
  int r;

  source = make_source(filename, value_type);

  rd.fd = -1;
  rd.rd = 0;
  rd.len = 0;
  rd.past = 0;
  rd.file = NULL;
  rd.buf = NULL;
  rd.filename = (char *) filename;
  qp_rd = &rd;

  if(strcmp(filename,"-") == 0)
  {
    rd.file = stdin;
    rd.fd = STDIN_FILENO;
  }

  if(rd.fd == -1)
    rd.fd = open(filename, O_RDONLY);

  if(rd.fd == -1)
  {
    EWARN("open(\"%s\",O_RDONLY) failed\n", filename);
    QP_EWARN("%sFailed to open file%s %s%s%s\n",
        bred, trm, btur, filename, trm);
    goto fail;
  }

  if(!is_pipe(&rd))
  {
    /* don't buffer read() and lseek() */
    qp_rd = NULL;
  }
  else
  {
    /* this is a pipe */
    DEBUG("Virturalizing a pipe%s%s\n",
        (filename[0] == '-' && filename[1] == '\0')?
        "":" with name ",
        (filename[0] == '-' && filename[1] == '\0')?
        "":filename);
    rd.buf = qp_malloc(BUF_LEN);
  }

  if((r = read_sndfile(source, &rd)))
  {
    if(r == -1)
      goto fail;


    if(rd.past && qp_rd)
    {
      VASSERT(0, "libsndfile read to much data "
          "to see that the file was not a sndfile\n");
      QP_WARN("%sFailed to read file%s %s%s%s:"
          " read wrapper failed\n",
          bred, trm, btur, filename, trm);
      goto fail;
    }

    if(qp_rd)
    {
      /* Start reading the data from the qp_rd read() buffer */
      rd.rd = 0;
      /* no need to add more to the qp_rd read() buffer */
      rd.fd = -1;
    }
    /* The above lseek() should work fine. */
    else if(lseek(rd.fd, 0, SEEK_SET))
    {
      EWARN("lseek(fd=%d, 0, SEEK_SET) failed\n", rd.fd);
      QP_EWARN("%sFailed to read file%s %s%s%s: lseek() failed\n",
          bred, trm, btur, filename, trm);
    }

    if(!rd.file)
    {
      errno = 0;
      rd.file = fdopen(rd.fd, "r");
      ASSERT(fileno(rd.file) == rd.fd);
    }
    if(!rd.file)
    {
      EWARN("fopen(\"%s\",\"r\") failed\n", filename);
      QP_EWARN("%sFailed to open file%s %s%s%s\n",
          bred, trm, btur, filename, trm);
      goto fail;
    }

    errno = 0;

    if(read_ascii(source, &rd))
      goto fail;
  }

  if(rd.buf)
  {
    free(rd.buf);
    rd.buf = NULL;
  }

  {
    /* remove any channels that have very bad data */
    struct qp_channel **c;
    size_t i = 0, chan_num = 0;
    ASSERT(source->channels);
    c = source->channels;
    while(c[i])
    {
      ASSERT(c[i]->form == QP_CHANNEL_FORM_SERIES);
      if(!is_good_double(c[i]->series.min) ||
          !is_good_double(c[i]->series.max))
      {
        struct qp_channel **n;
        
        qp_channel_destroy(c[i]);

        /* move of all pointers from c[i+1] back one */
        for(n=&c[i]; *n;++n)
          *n = *(n+1);

        /* re-malloc copying and removing one */
        source->channels = 
          qp_realloc(source->channels, sizeof(struct qp_channel *)*
              ((source->num_channels)--));
        
        /* reset c to the next one which is now at the
         * same index */
        c = source->channels;

        QP_NOTICE("removed bad channel number %zu\n", chan_num);
      }
      else
        ++i;
      ++chan_num;
    }
    ASSERT(source->num_channels == i);
  }
  
  
  if(source->num_channels == 0)
    goto fail;


  if(source->num_channels > 1)
  {
    /****** Check that there is at least one point in all the channels */
    ssize_t i, num;
    double *x;
    num = source->num_values;
    x = qp_malloc(sizeof(double)*source->num_channels);
    for(i=0;i<source->num_channels;++i)
      x[i] = qp_channel_series_double_begin(source->channels[i]);

    while(num)
    {
      int found = 0;
      for(i=0;i<source->num_channels;++i)
        if(is_good_double(x[i]))
          ++found;

      if(found >= 2)
        /* that means there is at least one x/y point
         * in all the channels. */
        break;

      --num;
      if(!num)
        break;

      for(i=0;i<source->num_channels;++i)
        x[i] = qp_channel_series_double_next(source->channels[i]);
    }

    if(!num)
    {
      QP_WARN("Failed to find a good point in data from file \"%s\"\n",
          filename);
      goto fail;
    }
  }

  
  if(source->num_channels == 0)
    goto fail;


  if(app->op_linear_channel || source->num_channels == 1)
  {
    /* Prepend a linear channel */

    /* TODO: Make this use less memory */
    
    struct qp_channel *c, **new_channels;
    double start = 0, step = 1;
    size_t len, i;

    if(app->op_linear_channel)
    {
      c = app->op_linear_channel;
      ASSERT(c->data);
      start = ((double*)c->data)[0];
      step = ((double*)c->data)[1];
    }
    else
    {
      c = qp_channel_linear_create(start, step);
    }


    len = source->num_values;
    for(i=0;i<len;++i)
      qp_channel_series_double_append(c, start + i*step);
   
    /* Prepend the channel to source->channels */
    /* reuse dummy len */
    len = source->num_channels + 1;
    new_channels = qp_malloc(sizeof(c)*len+1);
    new_channels[0] = c;
    for(i=1;i<len;++i)
      new_channels[i] = source->channels[i-1];
    new_channels[i] = NULL;
    free(source->channels);
    source->channels = new_channels;
    ++(source->num_channels);

    if(source->labels && source->num_labels !=  source->num_channels)
    {
      // shift the labels and add the linear channel label
      source->labels = qp_realloc(source->labels,
          sizeof(char *)*(source->num_labels+2));
      source->labels[source->num_labels+1] = NULL;
      for(i=source->num_labels;i>=1;--i)
        source->labels[i] = source->labels[i-1];

      char s[128];
      snprintf(s,128, "%s[0]", source->name);
      // The first channel is the linear channel.
      source->labels[0] = qp_strdup(s);
      ++source->num_labels;
    }

    /* Another source may have more values so
     * we must make a new one in case it is used again. */
    if(app->op_linear_channel)
      app->op_linear_channel = qp_channel_linear_create(start, step);
  }
  
  add_source_buffer_remove_menus(source);
  
  {
    char skip[64];
    skip[0] = '\0';
    if(app->op_skip_lines)
      snprintf(skip, 64, "(after skipping %zu) ", app->op_skip_lines);


    INFO("created source with %zu sets of values %s"
      "in %zu channels from file %s\n",
      source->num_values, skip, source->num_channels,
      filename);

    QP_INFO("created source with %zu sets of "
      "values %sin %zu channels from file \"%s\"\n",
      source->num_values, skip, source->num_channels,
      filename);
#if QP_DEBUG
    if(source->labels)
    {
      char **labels;
      APPEND("Read labels:");
      for(labels = source->labels; *labels; ++labels)
        APPEND(" \"%s\"", *labels);
      APPEND("\n");
    }
#endif
  }

  qp_rd = NULL;

  if(strcmp(filename,"-") == 0)
    /* We do not close stdin */
    return source;

  if(rd.file)
    fclose(rd.file);
  else if(rd.fd != -1)
    close(rd.fd);

  qp_app_graph_detail_source_remake();
  qp_app_set_window_titles();

  return source;

fail:

  QP_WARN("No data loaded from file \"%s\"\n",
      filename);

  if(rd.buf)
    free(rd.buf);

  if(strcmp(filename,"-") != 0)
  {
    if(rd.file)
      fclose(rd.file);
    else if(rd.fd != -1)
      close(rd.fd);
  }

  if(source)
    qp_source_destroy(source);

  qp_rd = NULL;

  return NULL;
}


qp_source_t qp_source_create_from_func(
    const char *name, int val_type,
    void (* func)(const void *))
{
  struct qp_source *source;
  source = make_source(name, val_type);

  /* TODO: add code here */

  add_source_buffer_remove_menus(source);

  qp_app_graph_detail_source_remake();
  qp_app_set_window_titles();

  return source;
}

static inline
void remove_source_menu_item(struct qp_win *qp,
    struct qp_source *source)
{
  GList *l, *l_alloc;
  l_alloc = (l =
      gtk_container_get_children(GTK_CONTAINER(qp->file_menu)));
  for(l=g_list_first(l); l; l=g_list_next(l))
  {
    /* The menu_item has a pointer to the source
     * in g_object_get_data(). */
    if(g_object_get_data(G_OBJECT(GTK_WIDGET(l->data)),
          "quickplot-source") == (void*) source)
    {
      /* this will remove from file menu */
      gtk_widget_destroy(GTK_WIDGET(l->data)); 
      break;
    }
  }
  if(l_alloc)
    /* no memory leak here */
    g_list_free(l_alloc);
}

/* returns 0 if there were no plots removed */
static inline
int remove_plots_from_graph(struct qp_graph *gr, struct qp_source *source)
{
  struct qp_plot *p;
  int ret = 0;

  p = qp_sllist_begin(gr->plots);
  while(p)
  {
     struct qp_channel **c;
     struct qp_plot *rp = NULL;
     for(c=source->channels; *c; ++c)
       if(qp_channel_equals(p->x, *c) ||
           qp_channel_equals(p->y, *c))
       {
          rp = p;
          break;
       }

     p = qp_sllist_next(gr->plots);

     if(rp)
     {
       ASSERT(rp != p);
       /* qp_sllist_remove() works so long as the
        * current plot, p, is not rp, and that
        * should be so since rp is the one before p. */
       qp_sllist_remove(gr->plots, rp, 0);
       qp_plot_destroy(rp, gr);
       ret = 1;
     }
  }
  return ret;
}

/* fix stuff when plots are removed from a graph
 * plots are removed because a source is removed. */
static inline
void fix_or_remove_changed_graph(struct qp_win *qp, struct qp_graph *gr)
{
  gint pnum;
  ASSERT(qp);
  ASSERT(qp->window);

  if(qp_sllist_length(gr->plots) == 0)
  {
    qp_graph_destroy(gr); /* remove */
    if(qp_sllist_length(qp->graphs) == 0)
    {
      struct qp_sllist_entry *current;
      /* we save and restore the list iterator
       * because qp_graph_create() uses the app qp list. */
      /* TODO: fix this Kludgey List Hack */
      current = app->qps->current;
      qp_graph_create(qp, NULL);
      app->qps->current = current;
    }
    return;
  }

  /* TODO: add rescale fixes due to max and min changes */

  /* fix it by queuing redraw if it's showing */
  pnum = gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook));
  gr->pixbuf_needs_draw = 1;
  if(gtk_notebook_get_nth_page(GTK_NOTEBOOK(qp->notebook), pnum)
      == gr->drawing_area)
    gtk_widget_queue_draw(qp->notebook);
}

void qp_source_destroy(qp_source_t source)
{
  ASSERT(source);
  ASSERT(source->name);
  ASSERT(source->channels);

  if(!source) return;


  { /* remove this source from buffers list from file menu
     * in all main windows (qp) */
    struct qp_win *qp;
  
    for(qp=qp_sllist_begin(app->qps);
      qp; qp=qp_sllist_next(app->qps))
    {
      if(qp->window)
        remove_source_menu_item(qp, source);
    
      {
        /* remove all plots that use the channels in this source. */
        struct qp_graph *gr;
        gr = qp_sllist_begin(qp->graphs);
        while(gr)
        {
          struct qp_graph *prev_gr;
          prev_gr = gr;
          gr = qp_sllist_next(qp->graphs);

          /* if we remove prev_gr from the qp_sllist in the
           * middle of this iteration it is okay because
           * we are not removing at the current list entry.
           * That is why we check the previous entry and
           * not the current. */
          if(remove_plots_from_graph(prev_gr, source))
            fix_or_remove_changed_graph(qp, prev_gr);
        }

      }
    }
  }

  {
    struct qp_channel **c;
    c = source->channels;
    for(c=source->channels; *c; ++c)
      qp_channel_destroy(*c);
    free(source->channels);
  }

  qp_sllist_remove(app->sources, source, 0);

  if(source->labels)
  {
    char **s;
    for(s = source->labels; *s; ++s)
    {
      if(*s)
        free(*s);
    }
    free(source->labels);
  }
    
  
  free(source->name);
  free(source);

  qp_app_graph_detail_source_remake();
  qp_app_set_window_titles();
}


#ifdef QP_DEBUG
void qp_source_debug_print(struct qp_source *source)
{
  struct qp_channel **c;
  size_t i = 0;
  ASSERT(source);
  DEBUG("\nQP source with %zu sets of values in "
      "%zu channels from file %s\n",
      source->num_values, source->num_channels,
      source->name);

  if(APPEND_ON())
  {
    for(c=source->channels; *c; ++c)
    {
      APPEND("Channel %zu =", i++);
      qp_channel_debug_append(*c);
      APPEND("\n");
    }
  }
}
#endif

