/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
 *
 * xpbs_datadump - (PBS) show stats of batch jobs, queues, or servers.
 *      - basically, this code borrows heavily from "qstat".
 *
 * Authors:
 *      Terry Heidelberg
 *      Livermore Computing
 *
 *      Bruce Kelly
 *      National Energy Research Supercomputer Center
 *
 *      Lawrence Livermore National Laboratory
 *      University of California
 *
 * Albeaus Bayucan
 * Sterling Software
 * NASA Ames Research Center
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "cmds.h"
#include <setjmp.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

static int myconnection;
static sigjmp_buf env_alrm;
static void
no_hang(int sig)
  {
  fprintf(stderr, "xpbs_datadump: alarm timed-out\n");
  myconnection = 0; /* connection to server  has failed */
  siglongjmp(env_alrm, 1);
  }

static char *mystrdup(

  char *str)

  {
  char *dup;

  dup = (char *)malloc((size_t)strlen(str) * sizeof(str));

  if (dup == NULL)
    {
    fprintf(stderr, "xpbs_datadump: out of memory\n");

    exit(2);
    }

  strcpy(dup, str);

  return(dup);
  }

void set_attrop(

  struct attropl **list,
  char            *a_name,
  char            *r_name,
  char            *v_name,
  enum batch_op    op)

  {

  struct attropl *attr;

  attr = (struct attropl *) malloc(sizeof(struct attropl));

  if (attr == NULL)
    {
    fprintf(stderr, "xpbs_datadump: out of memory\n");

    exit(2);
    }

  if (a_name == NULL)
    {
    attr->name = NULL;
    }
  else
    {
    attr->name = (char *) malloc(strlen(a_name) + 1);

    if (attr->name == NULL)
      {
      fprintf(stderr, "xpbs_datadump: out of memory\n");

      exit(2);
      }

    strcpy(attr->name, a_name);
    }

  if (r_name == NULL)
    {
    attr->resource = (char *)NULL;
    }
  else
    {
    attr->resource = (char *) malloc(strlen(r_name) + 1);

    if (attr->resource == NULL)
      {
      fprintf(stderr, "xpbs_datadump: out of memory\n");

      exit(2);
      }

    strcpy(attr->resource, r_name);
    }

  if (v_name == NULL)
    {
    attr->value = NULL;
    }
  else
    {
    attr->value = (char *) malloc(strlen(v_name) + 1);

    if (attr->value == NULL)
      {
      fprintf(stderr, "xpbs_datadump: out of memory\n");
      exit(2);
      }

    strcpy(attr->value, v_name);
    }

  attr->op = op;

  attr->next = *list;
  *list = attr;
  return;
  }



#define OPSTRING_LEN 4
#define OP_LEN 2
#define OP_ENUM_LEN 6
static char *opstring_vals[] = { "eq", "ne", "ge", "gt", "le", "lt" };
static enum batch_op opstring_enums[] = { EQ, NE, GE, GT, LE, LT };



void
check_op(char *optarg, enum batch_op *op, char *optargout)
  {
  char opstring[OP_LEN+1];
  int i;
  int cp_pos;

  *op = EQ;   /* default */
  cp_pos = 0;

  if (optarg[0] == '.')
    {
    strncpy(opstring, &optarg[1], OP_LEN);
    opstring[OP_LEN] = '\0';
    cp_pos = OPSTRING_LEN;

    for (i = 0; i < OP_ENUM_LEN; i++)
      {
      if (strncmp(opstring, opstring_vals[i], OP_LEN) == 0)
        {
        *op = opstring_enums[i];
        break;
        }
      }
    }

  strcpy(optargout, &optarg[cp_pos]);

  return;
  }



int
check_res_op(char *optarg, char *resource_name, enum batch_op *op, char *resource_value, char **res_pos)
  {
  char opstring[OPSTRING_LEN];
  int i;
  int hit;
  char *p;

  p = strchr(optarg, '.');

  if (p == NULL || *p == '\0')
    {
    fprintf(stderr, "xpbs_datadump: illegal -l value\n");
    fprintf(stderr, "resource_list: %s\n", optarg);
    return (1);
    }
  else
    {
    strncpy(resource_name, optarg, p - optarg);
    resource_name[p-optarg] = '\0';
    *res_pos = p + OPSTRING_LEN;
    }

  if (p[0] == '.')
    {
    strncpy(opstring, &p[1] , OP_LEN);
    opstring[OP_LEN] = '\0';
    hit = 0;

    for (i = 0; i < OP_ENUM_LEN; i++)
      {
      if (strncmp(opstring, opstring_vals[i], OP_LEN) == 0)
        {
        *op = opstring_enums[i];
        hit = 1;
        break;
        }
      }

    if (! hit)
      {
      fprintf(stderr, "xpbs_datadump: illegal -l value\n");
      fprintf(stderr, "resource_list: %s\n", optarg);
      return (1);
      }
    }

  p = strchr(*res_pos, ',');

  if (p == NULL)
    {
    p = strchr(*res_pos, '\0');
    }

  strncpy(resource_value, *res_pos, p - (*res_pos));

  resource_value[p-(*res_pos)] = '\0';

  if (strlen(resource_value) == 0)
    {
    fprintf(stderr, "xpbs_datadump: illegal -l value\n");
    fprintf(stderr, "resource_list: %s\n", optarg);
    return (1);
    }

  *res_pos = (*p == '\0') ? p : (p += 1) ;

  if (**res_pos == '\0' && *(p - 1) == ',')
    {
    fprintf(stderr, "xpbs_datadump: illegal -l value\n");
    fprintf(stderr, "resource_list: %s\n", optarg);
    return (1);
    }

  return(0);  /* ok */
  }

int
istrue(char *string)
  {
  if (strcmp(string, "TRUE") == 0) return TRUE;

  if (strcmp(string, "True") == 0) return TRUE;

  if (strcmp(string, "true") == 0) return TRUE;

  if (strcmp(string, "1") == 0) return TRUE;

  return FALSE;
  }




void
states(char *string, char *q, char *r, char *h, char *w, char *t, char *e, char *comp, int len)
  {
  char *c, *d, *f, *s, l;

  c = string;

  while (isspace(*c) && *c != '\0') c++;

  while (*c != '\0')
    {
    s = c;

    while (*c != ':') c++;

    *c = '\0';

    d = NULL;

    if (strcmp(s, "Queued") == 0) d = q;
    else if (strcmp(s, "Running") == 0) d = r;
    else if (strcmp(s, "Held")    == 0) d = h;
    else if (strcmp(s, "Waiting") == 0) d = w;
    else if (strcmp(s, "Transit") == 0) d = t;
    else if (strcmp(s, "Exiting") == 0) d = e;
    else if (strcmp(s, "Complete") == 0) d = comp;

    c++;

    if (d != NULL)
      {
      s = c;

      while (*c != ' ' && *c != '\0') c++;

      l = *c;

      *c = '\0';

      if (strlen(s) > (size_t)len)
        {
        f = s + len;
        *f = '\0';
        }

      strcpy(d, s);

      if (l != '\0') c++;
      }
    }
  }


/*
 * print a attribute value string, formating to break a comma if possible
 */

void
prt_attr(char *n, char *r, char *v)
  {
  char *c;
  char *comma = ",";
  int   first = 1;
  int   l;
  int   start;

  start = strlen(n) + 7; /* 4 spaces + ' = ' is 7 */
  printf("    %s", n);

  if (r)
    {
    start += strlen(r) + 1;
    printf(".%s", r);
    }

  printf(" = ");

  c = strtok(v, comma);

  while (c)
    {
    if ((l = strlen(c)) + start < 78)
      {
      printf("%s", c);
      start += l;
      }
    else
      {
      if (! first)
        {
        printf("\n\t");
        start = 9;
        }

      while (*c)
        {
        putchar(*c++);

        if (++start > 78)
          {
          start = 8;
          printf("\n\t");
          }
        }
      }

    if ((c = strtok((char *)0, comma)))
      {
      first = 0;
      putchar(',');
      }
    }
  }

#define NAMEL   12  /* printf of jobs, queues, and servers */
#define NODEL   5
#define OWNERL  8   /* printf of jobs */
#define TIMEUL  8   /* printf of jobs */
#define STATEL  1   /* printf of jobs */
#define LOCL    12  /* printf of jobs */

void
display_statjob(struct batch_status *status, int prtheader, int full, char *server_name)
  {

  struct batch_status *p;

  struct attrl *a;
  int l;
  char *c, *e = NULL;
  char jid[PBS_MAXSEQNUM+14+1];
  char name[NAMEL+1];
  char owner[OWNERL+1];
  char nodes[NODEL+1];
  char cputimeu[TIMEUL+1];
  char walltimeu[TIMEUL+1];
  char state[STATEL+1];
  char location[LOCL+1];
  char format[80];
  char format2[80];
  char long_name[17];
  time_t epoch;

  sprintf(format, "%%-%ds %%-%ds %%-%ds %%%ds %%%ds  %%%ds %%%ds  %%s@%%s %%s %%s\n",
          PBS_MAXSEQNUM + 14, NAMEL, OWNERL, NODEL, TIMEUL, TIMEUL, STATEL);
  sprintf(format2, "%%-%ds %%-%ds %%-%ds %%%ds %%%ds  %%%ds %%%ds  %%s\n",
          PBS_MAXSEQNUM + 14, NAMEL, OWNERL, NODEL, TIMEUL, TIMEUL, STATEL);

  if (! full && prtheader)
    {
    printf(":");
    printf(format2, "Job id", "Name", "User", "PEs",  "CputUse",  "WalltUse", "S",  "Queue");
    }

  p = status;

  while (p != NULL)
    {
    jid[0] = '\0';
    name[0] = '\0';
    owner[0] = '\0';
    cputimeu[0] = '\0';
    walltimeu[0] = '\0';
    state[0] = '\0';
    location[0] = '\0';
    nodes[0] = '\0';

    if (full)
      {
      printf("Job Id: %s@%s\n", p->name, server_name);
      a = p->attribs;

      while (a != NULL)
        {
        if (a->name != NULL)
          {
          if (strcmp(a->name, ATTR_ctime) == 0 ||
              strcmp(a->name, ATTR_mtime) == 0 ||
              strcmp(a->name, ATTR_qtime) == 0 ||
              strcmp(a->name, ATTR_a) == 0)
            {
            epoch = (time_t) atoi(a->value);
            prt_attr(a->name, a->resource, ctime(&epoch));
            }
          else
            {
            prt_attr(a->name, a->resource, a->value);
            printf("\n");
            }
          }

        a = a->next;
        }
      }
    else
      {


      if (p->name != NULL)
        {
        if ((e = mystrdup(p->name)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e;  /* we need e as the starting address for */

        /* future free */

        while (*c != '.' && *c != '\0') c++;

        c++;    /* List the first part of the server name, too. */

        while (*c != '.' && *c != '\0') c++;

        c++;    /* List also the 2nd part of the server name, too. */

        while (*c != '.' && *c != '\0') c++;

        *c = '\0';

        l = strlen(e);

        if (l > (PBS_MAXSEQNUM + 14))
          {
          c  = e + PBS_MAXSEQNUM + 14;
          *c = '\0';
          }

        strcpy(jid, e);

        free(e);
        }

      a = p->attribs;

      while (a != NULL)
        {
        if (a->name != NULL)
          {
          if ((e = mystrdup(a->value)) == NULL)
            {
            fprintf(stderr, "Error getting more space to malloc\n");
            exit(2);
            }

          c = e; /* we need e as the starting address for */

          /* future free */

          if (strcmp(a->name, ATTR_name) == 0)
            {
            l = strlen(e);

            if (l > NAMEL)
              {
              l = l - NAMEL + 3;
              c = e + l;

              while (*c != '/' && *c != '\0') c++;

              if (*c == '\0') c = e + l;

              strcpy(long_name, "...");

              strcat(long_name, c);

              c = long_name;
              }
            else
              c = e;

            strcpy(name, c);
            }
          else if (strcmp(a->name, ATTR_owner) == 0)
            {
            c = e;

            while (*c != '@' && *c != '\0') c++;

            *c = '\0';

            l = strlen(e);

            if (l > OWNERL)
              {
              c = e + OWNERL;
              *c = '\0';
              }

            strcpy(owner, e);
            }
          else if (strcmp(a->name, ATTR_used) == 0)
            {
            if (strcmp(a->resource, "cput") == 0)
              {
              l = strlen(e);

              if (l > TIMEUL)
                {
                c = e + TIMEUL;
                *c = '\0';
                }

              strcpy(cputimeu, e);
              }
            else if (strcmp(a->resource, "walltime") == 0)
              {
              l = strlen(e);

              if (l > TIMEUL)
                {
                c = e + TIMEUL;
                *c = '\0';
                }

              strcpy(walltimeu, e);
              }
            }
          else if (strcmp(a->name, ATTR_state) == 0)
            {
            l = strlen(e);

            if (l > STATEL)
              {
              c = e + STATEL;
              *c = '\0';
              }

            strcpy(state, e);
            }
          else if (strcmp(a->name, ATTR_queue) == 0)
            {
            c = e;

            while (*c != '@' && *c != '\0') c++;

            *c = '\0';

            l = strlen(e);

            if (l > LOCL)
              {
              c = e + LOCL;
              *c = '\0';
              }

            strcpy(location, e);
            }
          else if (strcmp(a->name, ATTR_l) == 0)
            {
            if (strcmp(a->resource, "nodect") == 0 || \
                strcmp(a->resource, "ncpus") == 0)
              {
              l = strlen(e);

              if (l > NODEL)
                {
                c = e + NODEL;
                *c = '\0';
                }

              strcpy(nodes, e);
              }
            }
          }

        a = a->next;

        free(e);
        }

      if (cputimeu[0] == '\0')
        strcpy(cputimeu, "0");

      if (walltimeu[0] == '\0')
        strcpy(walltimeu, "0");

      if (nodes[0] == '\0')
        strcpy(nodes, "-");

      printf(format, jid, name, owner, nodes, cputimeu, walltimeu, state,
             location, server_name, server_name, p->name);
      }

    if (full) printf("\n");

    p = p->next;
    }

  return;
  }

void
display_trackstatjob(struct batch_status *status, char *server_name)
  {

  struct batch_status *p;

  struct attrl *a;
  char owner[OWNERL+1];
  char output_path[MAXPATHLEN];
  char error_path[MAXPATHLEN];
  int l;
  char *c, *e = NULL;

  p = status;

  while (p != NULL)
    {
    printf("%s@%s ", p->name, server_name);
    a = p->attribs;

    while (a != NULL)
      {
      if (a->name != NULL)
        {
        if ((e = mystrdup(a->value)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e; /* we need e as the starting address for */

        /* future free */

        if (strcmp(a->name, ATTR_owner) == 0)
          {
          c = e;

          while (*c != '@' && *c != '\0') c++;

          *c = '\0';

          l = strlen(e);

          if (l > OWNERL)
            {
            c = e + OWNERL;
            *c = '\0';
            }

          strcpy(owner, e);
          }
        else if (strcmp(a->name, ATTR_o) == 0)
          {
          strcpy(output_path, a->value);
          }
        else if (strcmp(a->name, ATTR_e) == 0)
          {
          strcpy(error_path, a->value);
          }
        }

      free(e);

      a = a->next;
      }

    printf("%s %s %s\n", owner, output_path, error_path);

    p = p->next;
    }

  return;
  }

#define QNAMEL  16
#define NUML    5
#define TYPEL   10

void
display_statque(struct batch_status *status, int prtheader, int full, char *server_name)
  {

  struct batch_status *p;

  struct attrl *a;
  int l;
  char *c, *e;
  char name[QNAMEL+1];
  char max[NUML+1];
  char tot[NUML+1];
  char ena[NUML+1];
  char str[NUML+1];
  char que[NUML+1];
  char run[NUML+1];
  char hld[NUML+1];
  char wat[NUML+1];
  char trn[NUML+1];
  char ext[NUML+1];
  char comp[NUML+1];
  char type[TYPEL+1];
  char format[80];

  sprintf(format, "%%-%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%-%ds %%s\n",
          QNAMEL, NUML, NUML, NUML, NUML, NUML,
          NUML, NUML, NUML, NUML, NUML, TYPEL);

  if (! full && prtheader)
    {
    printf(":");
    printf(format, "Queue", "Max", "Tot", "Ena", "Str", "Que", "Run", "Hld", "Wat", "Trn", "Ext", "Type", "Server");
    }

  p = status;

  while (p != NULL)
    {
    name[0] = '\0';
    strcpy(max, "0");
    strcpy(tot, "0");
    strcpy(ena, "no");
    strcpy(str, "no");
    strcpy(que, "0");
    strcpy(run, "0");
    strcpy(hld, "0");
    strcpy(wat, "0");
    strcpy(trn, "0");
    strcpy(ext, "0");
    strcpy(comp, "0");
    type[0] = '\0';

    if (full)
      {
      printf("Queue: %s@%s\n", p->name, server_name);
      a = p->attribs;

      while (a != NULL)
        {
        if (a->name != NULL)
          {
          prt_attr(a->name, a->resource, a->value);
          printf("\n");
          }

        a = a->next;
        }
      }
    else
      {
      if (p->name != NULL)
        {
        if ((e = mystrdup(p->name)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e;  /* we need e as the starting address for */

        /* future free */

        l = strlen(e);

        if (l > NAMEL)
          {
          c = e + NAMEL;
          *c = '\0';
          }

        strcpy(name, e);

        free(e);
        }

      a = p->attribs;

      while (a != NULL)
        {
        if ((e = mystrdup(a->value)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e;  /* we need e as the starting address for */

        /* future free */

        if (a->name != NULL)
          {
          if (strcmp(a->name, ATTR_maxrun) == 0)
            {
            l = strlen(e);

            if (l > NUML)
              {
              c = e + NUML;
              *c = '\0';
              }

            strcpy(max, e);
            }
          else if (strcmp(a->name, ATTR_total) == 0)
            {
            l = strlen(e);

            if (l > NUML)
              {
              c = e + NUML;
              *c = '\0';
              }

            strcpy(tot, e);
            }
          else if (strcmp(a->name, ATTR_enable) == 0)
            {
            if (istrue(e))
              strcpy(ena, "yes");
            else
              strcpy(ena, "no");
            }
          else if (strcmp(a->name, ATTR_start) == 0)
            {
            if (istrue(e))
              strcpy(str, "yes");
            else
              strcpy(str, "no");
            }
          else if (strcmp(a->name, ATTR_count) == 0)
            {
            states(e, que, run, hld, wat, trn, ext, comp, NUML);
            }
          else if (strcmp(a->name, ATTR_qtype) == 0)
            {
            l = strlen(e);

            if (l > TYPEL)
              {
              c = e + TYPEL;
              *c = '\0';
              }

            strcpy(type, e);
            }
          }

        a = a->next;

        free(e);
        }

      printf(format, name, max, tot, ena, str, que, run, hld, wat, trn, ext, type, server_name);
      }

    if (full) printf("\n");

    p = p->next;
    }

  return;
  }

#define SERVERL 23
#define STATUSL 10
#define NODESL  9

void
display_statserver(struct batch_status *status, int prtheader, int full, int nodesInUse)
  {

  struct batch_status *p;

  struct attrl *a;
  int l;
  char *c, *e;
  char name[SERVERL+1];
  char max[NUML+1];
  char tot[NUML+1];
  char que[NUML+1];
  char run[NUML+1];
  char hld[NUML+1];
  char wat[NUML+1];
  char trn[NUML+1];
  char ext[NUML+1];
  char comp[NUML+1];
  char nod[NODESL+1];
  char stats[STATUSL+1];
  char format[80];

  sprintf(format, "%%-%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%%ds %%-%ds %%%ds                    %%s\n", SERVERL, NUML, NUML, NUML, NUML, NUML, NUML, NUML, NUML, STATUSL, NODESL);

  if (! full && prtheader)
    {
    printf(":");
    printf(format, "Server", "Max", "Tot", "Que", "Run", "Hld", "Wat", "Trn", "Ext", "Status", "PEsInUse", "");
    }

  p = status;

  while (p != NULL)
    {
    name[0] = '\0';
    strcpy(max, "0");
    strcpy(tot, "0");
    strcpy(que, "0");
    strcpy(run, "0");
    strcpy(hld, "0");
    strcpy(wat, "0");
    strcpy(trn, "0");
    strcpy(ext, "0");
    strcpy(comp, "0");
    strcpy(nod, "-/-");
    stats[0] = '\0';

    if (full)
      {
      printf("Server: %s\n", p->name);
      a = p->attribs;

      while (a != NULL)
        {
        if (a->name != NULL)
          {
          prt_attr(a->name, a->resource, a->value);
          printf("\n");
          }

        a = a->next;
        }
      }
    else
      {
      if (p->name != NULL)
        {
        if ((e = mystrdup(p->name)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e;  /* we need e as the starting address for */

        /* future free */

        l = strlen(e);

        if (l > SERVERL)
          {
          c = e + SERVERL;
          *c = '\0';
          }

        strcpy(name, e);

        free(e);
        }

      a = p->attribs;

      while (a != NULL)
        {

        if ((e = mystrdup(a->value)) == NULL)
          {
          fprintf(stderr, "Error getting more space to malloc\n");
          exit(2);
          }

        c = e;  /* we need e as the starting address for */

        /* future free */

        if (a->name != NULL)
          {
          if (strcmp(a->name, ATTR_maxrun) == 0)
            {
            l = strlen(e);

            if (l > NUML)
              {
              c = e + NUML;
              *c = '\0';
              }

            strcpy(max, e);
            }
          else if (strcmp(a->name, ATTR_total) == 0)
            {
            l = strlen(e);

            if (l > NUML)
              {
              c = e + NUML;
              *c = '\0';
              }

            strcpy(tot, e);
            }
          else if (strcmp(a->name, ATTR_count) == 0)
            {
            states(e, que, run, hld, wat, trn, ext, comp, NUML);
            }
          else if (strcmp(a->name, ATTR_status) == 0)
            {
            l = strlen(e);

            if (l > STATUSL)
              {
              c = e + STATUSL;
              *c = '\0';
              }

            strcpy(stats, e);
            }
          else if (strcmp(a->name, ATTR_rescmax) == 0)
            {
            if (strcmp(a->resource, "nodect") == 0 || \
                strcmp(a->resource, "ncpus") == 0)
              {
              l = strlen(e);

              if (l > NODESL)
                {
                c = e + NODESL;
                *c = '\0';
                }

              sprintf(nod, "%d/%s", nodesInUse, e);
              }
            }
          }

        a = a->next;

        free(e);
        }

      printf(format, name, max, tot, que, run, hld, wat, trn, ext, stats,

             nod, p->name ? p->name : "");
      }

    if (full) printf("\n");

    p = p->next;
    }

  return;
  }

static int getNumNodesInUse(
    
  int myconnection)

  {
  struct batch_status *j_status;
  struct batch_status *temp;
  struct attrl        *attr;

  struct attropl *run_list = 0;
  int  nodesInUse = 0;
  char *errmsg;
  int  nodect;
  int  cpuct;
  int  nodes;
  int  local_errno = 0;

  set_attrop(&run_list, ATTR_state, (char *)NULL, "R", EQ);
  j_status = pbs_selstat_err(myconnection, run_list, NULL, &local_errno);

  if (j_status == NULL)
    {
    if (local_errno != PBSE_NONE)
      {
      errmsg = pbs_geterrmsg(myconnection);

      if (errmsg != NULL)
        {
        fprintf(stderr, "xpbs_datadump: %s\n", errmsg);
        }
      else
        {
        fprintf(stderr, "xpbs_datadump: Error (%d) selecting running jobs to obtain nodes in use value\n", local_errno);
        }

      return(-1);
      }
    }
  else     /* got some output */
    {
    for (temp = j_status; temp; temp = temp->next)
      {
      nodect = 0;  /* start with a nodect of 0 */
      cpuct = 0;  /* start with a cpuct of 0 */
      nodes = 0;  /* nodes is not set */

      for (attr = temp->attribs; attr; attr = attr->next)
        {
        if (strcmp(attr->name, ATTR_l) == 0)
          {
          if (strcmp(attr->resource, "nodect") == 0)
            {
            nodect = atoi(attr->value);
            }
          else if (strcmp(attr->resource, "nodes") == 0)
            {
            nodes = 1; /* nodes is set */
            }
          else if (strcmp(attr->resource, "ncpus") == 0)
            {
            cpuct = atoi(attr->value);
            }
          }
        }

      if (nodes)
        {
        nodesInUse += nodect;
        }
      else
        {
        nodesInUse += cpuct;
        }
      }

    pbs_statfree(j_status);
    }

  return(nodesInUse);
  }

int main(  /* qstat */

  int    argc,
  char **argv)

  {
  int c;
  int errflg = 0;
  int any_failed = 0;

  char server_out[MAXSERVERNAME];
  char full_server_name[MAXSERVERNAME];

  char *queue_name_out = NULL;

  struct batch_status *p_status;
  char *errmsg;


#define MAX_OPTARG_LEN 256
#define MAX_RESOURCE_NAME_LEN 256

  char optargout[MAX_OPTARG_LEN+1];
  char resource_name[MAX_RESOURCE_NAME_LEN+1];

  enum batch_op op;
  enum batch_op *pop = &op;

  struct attropl *select_list = 0;

  static char destination[PBS_MAXQUEUENAME+1] = "";

  char *server_name_out;

  char *res_pos;
  char *pc;
  int u_cnt, o_cnt, s_cnt, n_cnt;
  time_t after;
  char a_value[80];
  volatile int do_job_only = FALSE;
  volatile int do_trackjob_only = FALSE;
  volatile int timeout_secs = 30; /* # of seconds before timing out waiting */
  /* for a connection to the server */

  struct sigaction act;

#define GETOPT_ARGS "a:A:c:h:l:N:p:q:r:s:u:JTt:"

  while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    switch (c)
      {

      case 't':
        timeout_secs = atoi(optarg);
        break;

      case 'T':
        do_trackjob_only = TRUE;
        break;

      case 'J':
        do_job_only = TRUE;
        break;

      case 'a':
        check_op(optarg, pop, optargout);

        if ((after = cvtdate(optargout)) < 0)
          {
          fprintf(stderr, "xpbs_datadump: illegal -a value\n");
          errflg++;
          break;
          }

        sprintf(a_value, "%ld", (long)after);

        set_attrop(&select_list, ATTR_a, (char *)NULL, a_value, op);
        break;

      case 'c':
        check_op(optarg, pop, optargout);
        pc = optargout;

        while (isspace((int)*pc)) pc++;

        if (strlen(pc) == 0)
          {
          fprintf(stderr, "xpbs_datadump: illegal -c value\n");
          errflg++;
          break;
          }

        if (strcmp(pc, "u") == 0)
          {
          if ((op != EQ) && (op != NE))
            {
            fprintf(stderr, "xpbs_datadump: illegal -c value\n");
            errflg++;
            break;
            }
          }
        else if ((strcmp(pc, "n") != 0) &&
                 (strcmp(pc, "s") != 0) &&
                 (strcmp(pc, "c") != 0))
          {
          if (strncmp(pc, "c=", 2) != 0)
            {
            fprintf(stderr, "xpbs_datadump: illegal -c value\n");
            errflg++;
            break;
            }

          pc += 2;

          if (strlen(pc) == 0)
            {
            fprintf(stderr, "xpbs_datadump: illegal -c value\n");
            errflg++;
            break;
            }

          while (*pc != '\0')
            {
            if (!isdigit((int)*pc))
              {
              fprintf(stderr, "xpbs_datadump: illegal -c value\n");
              errflg++;
              break;
              }

            pc++;
            }
          }

        set_attrop(&select_list, ATTR_c, (char *)NULL, optargout, op);

        break;

      case 'h':
        check_op(optarg, pop, optargout);
        pc = optargout;

        while (isspace((int)*pc)) pc++;

        if (strlen(pc) == 0)
          {
          fprintf(stderr, "xpbs_datadump: illegal -h value\n");
          errflg++;
          break;
          }

        u_cnt = o_cnt = s_cnt = n_cnt = 0;

        while (*pc)
          {
          if (*pc == 'u')
            u_cnt++;
          else if (*pc == 'o')
            o_cnt++;
          else if (*pc == 's')
            s_cnt++;
          else if (*pc == 'n')
            n_cnt++;
          else
            {
            fprintf(stderr, "xpbs_datadump: illegal -h value\n");
            errflg++;
            break;
            }

          pc++;
          }

        if (n_cnt && (u_cnt + o_cnt + s_cnt))
          {
          fprintf(stderr, "xpbs_datadump: illegal -h value\n");
          errflg++;
          break;
          }

        set_attrop(&select_list, ATTR_h, (char *)NULL, optargout, op);

        break;

      case 'l':
        res_pos = optarg;

        while (*res_pos != '\0')
          {
          if (check_res_op(res_pos, resource_name, pop, optargout, &res_pos) != 0)
            {
            errflg++;
            break;
            }

          set_attrop(&select_list, ATTR_l, resource_name, optargout, op);
          }

        break;

      case 'p':
        check_op(optarg, pop, optargout);
        set_attrop(&select_list, ATTR_p, (char *)NULL, optargout, op);
        break;

      case 'q':
        strcpy(destination, optarg);
        check_op(optarg, pop, optargout);
        set_attrop(&select_list, ATTR_q, (char *)NULL, optargout, op);
        break;

      case 'r':
        op = EQ;
        pc = optarg;

        while (isspace((int)(*pc))) pc++;

        if (strlen(pc) != 1)
          {
          fprintf(stderr, "xpbs_datadump: illegal -r value\n");
          errflg++;
          break;
          }

        if (*pc != 'y' && *pc != 'n')
          {
          fprintf(stderr, "xpbs_datadump: illegal -r value\n");
          errflg++;
          break;
          }

        set_attrop(&select_list, ATTR_r, (char *)NULL, pc, op);

        break;

      case 's':
        check_op(optarg, pop, optargout);
        pc = optargout;

        while (isspace((int)(*pc))) pc++;

        if (strlen(optarg) == 0)
          {
          fprintf(stderr, "xpbs_datadump: illegal -s value\n");
          errflg++;
          break;
          }

        while (*pc)
          {
          if (*pc != 'E' && *pc != 'H' && *pc != 'Q' &&
              *pc != 'R' && *pc != 'T' && *pc != 'W')
            {
            fprintf(stderr, "xpbs_datadump: illegal -s value\n");
            errflg++;
            break;
            }

          pc++;
          }

        set_attrop(&select_list, ATTR_state, (char *)NULL, optargout, op);

        break;

      case 'u':
        op = EQ;

        if (parse_at_list(optarg, FALSE, FALSE))
          {
          fprintf(stderr, "xpbs_datadump: illegal -u value\n");
          errflg++;
          break;
          }

        set_attrop(&select_list, ATTR_u, (char *)NULL, optarg, op);

        break;

      case 'A':
        op = EQ;
        set_attrop(&select_list, ATTR_A, (char *)NULL, optarg, op);
        break;

      case 'N':
        op = EQ;
        set_attrop(&select_list, ATTR_N, (char *)NULL, optarg, op);
        break;

      default :
        errflg++;
      }

  if (errflg || (optind == argc))
    {
    static char usage[] = "usage: xpbs_datadump \
                          [-a [op]date_time] [-A account_string] [-c [op]interval] \n\
                          [-h hold_list] [-l resource_list] [-N name] [-p [op]priority] \n\
                          [-q destination] [-r y|n] [-s states] [-u user_name] [-J] [-T] \n\
                          [-t timeout_secs] server_name..\n";
    fprintf(stderr, "%s", usage);
    exit(2);
    }

  if (notNULL(destination))
    {
    if (parse_destination_id(destination, &queue_name_out, &server_name_out))
      {
      fprintf(stderr, "xpbs_datadump: illegally formed destination: %s\n", destination);
      exit(2);
      }
    else
      {
      if (notNULL(server_name_out))
        {
        strcpy(server_out, server_name_out);
        }
      }
    }


  act.sa_handler = no_hang;

  sigemptyset(&act.sa_mask);

#ifdef SA_INTERRUPT
  act.sa_flags   = SA_INTERRUPT;
#else
  act.sa_flags   = 0;
#endif /* SA_INTERRUPT */

  (void)sigaction(SIGALRM, &act, (struct sigaction *)0);

  for (; optind < argc; optind++)
    {
    strcpy(server_out, argv[optind]);

    if (sigsetjmp(env_alrm, 1) == 0)
      {
      alarm(timeout_secs);
      myconnection = cnt2server(server_out);
      }

    alarm(0);

    if (myconnection <= 0)
      {
      fprintf(stderr, "xpbs_datadump: Can not connect to server %s \n",
        server_out);
      any_failed = myconnection;
      continue;
      }

    /* Get server information */
    p_status = pbs_statserver_err(myconnection, NULL, NULL, &any_failed);

    if (p_status == NULL)
      {

      if (any_failed)
        {
        errmsg = pbs_geterrmsg(myconnection);

        if (errmsg != NULL)
          {
          fprintf(stderr, "qstat: %s ", errmsg);
          }
        else
          fprintf(stderr, "xpbs_datadump: Error (%d) getting status of server ", any_failed);

        fprintf(stderr, "%s\n", server_out);
        }
      }
    else
      {
      strcpy(full_server_name, p_status->name);

      if (!do_job_only && !do_trackjob_only)
        {
        display_statserver(p_status, TRUE, FALSE,
                           getNumNodesInUse(myconnection));
        }

      pbs_statfree(p_status);
      }

    /* Get the queue information */
    p_status = pbs_statque_err(myconnection, queue_name_out, NULL, NULL, &any_failed);

    if (p_status == NULL)
      {
      if (any_failed)
        {
        errmsg = pbs_geterrmsg(myconnection);

        if (errmsg != NULL)
          {
          fprintf(stderr, "qstat: %s ", errmsg);
          }
        else
          fprintf(stderr, "xpbs_datadump: Error (%d) getting status of queue ", any_failed);

        fprintf(stderr, "%s\n", queue_name_out);
        }
      }
    else
      {
      if (!do_job_only && !do_trackjob_only)
        {
        display_statque(p_status, TRUE, FALSE, full_server_name);
        }

      pbs_statfree(p_status);
      }

    /*
    ### Get Jobs summary info information for each of the servers
    */
    p_status = pbs_selstat_err(myconnection, select_list, NULL, &any_failed);

    if (p_status == NULL)
      {
      if (any_failed != PBSE_NONE)
        {
        errmsg = pbs_geterrmsg(myconnection);

        if (errmsg != NULL)
          {
          fprintf(stderr, "xpbs_datadump: %s\n", errmsg);
          }
        else
          {
          fprintf(stderr, "xpbs_datadump: Error (%d) selecting jobs\n", any_failed);
          }
        }
      }
    else     /* got some output */
      {
      if (!do_trackjob_only)
        {
        display_statjob(p_status, TRUE, FALSE, full_server_name);
        }
      else
        {
        display_trackstatjob(p_status, full_server_name);
        }

      pbs_statfree(p_status);
      }

    pbs_disconnect(myconnection);
    }

  exit(any_failed);
  }
