/*-
 * Copyright (c) 1992, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char const sccsid[] = "@(#)util.c 8.2 (Berkeley) 4/2/94";
#endif /* not lint */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif

#include "extern.h"
#include "pathnames.h"

static char const ident[] = "@(#) $RCSfile$ $Revision: 5419 $";

char *
colon(char *cp)
  {
  if (*cp == ':')  /* Leading colon is part of file name. */
    return (0);

  for (; *cp; ++cp)
    {
    if (*cp == ':')
      return (cp);

    if (*cp == '/')
      return (0);
    }

  return (0);
  }

void
verifydir(char *cp)
  {

  struct stat stb;

  if (!stat(cp, &stb))
    {
    if (S_ISDIR(stb.st_mode))
      return;

    errno = ENOTDIR;
    }

  run_err("%s: %s", cp, strerror(errno));

  exit(1);
  }

int
okname(char *cp0)
  {
  int c;
  char *cp;

  cp = cp0;

  do
    {
    c = *cp;

    if (c & 0200)
      goto bad;

    if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
      goto bad;
    }
  while (*++cp);

  return (1);

bad:
  warnx("%s: invalid user name", cp0);

  return (0);
  }

int
susystem(char *s, int userid)
  {
  int status;
  pid_t pid;

  pid = fork();

  switch (pid)
    {

    case - 1:
      return (127);

    case 0:
      if (setuid(userid) != 0)
        {
      	run_err("setuid(%ld): %s", (long)userid,
      	strerror(errno));
      	return (127);
      	}
      execl(_PATH_BSHELL, "sh", "-c", s, NULL);
      _exit(127);
    }

  if (waitpid(pid, &status, 0) < 0)
    status = -1;

  return (status);
  }




BUF *allocbuf(

  BUF *bp,
  int  fd, 
  int  blksize)

  {

  struct stat stb;
  size_t size;

  void *tmpP;

  if (fstat(fd, &stb) < 0)
    {
    run_err("fstat: %s", strerror(errno));

    return (0);
    }

  size = (((int)stb.st_blksize + blksize - 1) / blksize) * blksize;

  if (size == 0)
    size = blksize;

  if (bp->cnt >= (int)size)
    {
    return(bp);
    }

  if ((tmpP = calloc(1, size)) == NULL)
    {
    bp->cnt = 0;

    run_err("%s",strerror(errno));

    return(0);
    }
  if (bp->buf != NULL)
    strcat(tmpP, bp->buf);
  free(bp->buf);

  bp->buf = tmpP;

  bp->cnt = size;

  return (bp);
  }





void lostconn(

  int signo)

  {
  if (!iamremote)
    warnx("lost connection");

  exit(1);
  }
