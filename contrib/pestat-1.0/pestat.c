/*

David.Singleton@anu.edu.au       ANU Supercomputer Facility
Phone: +61 2 6249 4389           Australian National University
Fax: +61 2 6279 8199             Canberra, ACT, 0200, Australia

*/


#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pbs_error.h>

#include "portability.h"
#include "pbs_ifl.h"
#include <rm.h>

#define MIN(a,b) ( (a) > (b) ? (b) : (a) )

#define DOWN    0
#define LIST    1
#define CLEAR   2
#define OFFLINE 3
#define RESET   4
#define ALL     5
#define TRUE    1

/* node-attribute values (state,ntype) */

#define ND_free                 "free"
#define ND_offline              "offline"
#define ND_down                 "down"
#define ND_reserve              "reserve"
#define ND_job_exclusive        "job-exclusive"
#define ND_job_sharing          "job-sharing"
#define ND_state_unknown        "state-unknown"
#define ND_timeshared           "time-shared"
#define ND_cluster              "cluster"

static int DEBUG = 0;
struct state_map
  {
  char *rname;
  char *p_rname;
  };

struct state_map pstate[] =
  {
    { "free",          "free"
    },

  { "offline",       "off " },
  { "down",          "down" },
  { "reserve",       "rsrv" },
  { "job-exclusive", "excl" },
  { "job-sharing",   "shrd" },
  { "state-unknown", "unkn" },
  { "time-shared",   "ts  " },
  { "cluster",       "clus" },
  { NULL,            NULL   }
  };

int quiet = 0;


const char *res_to_get[] =
  {
  "loadave", /* the current load average */
  "physmem", /* total memory size in KB */
  "ncpus", /* number of cpus */
#ifdef TRU64
  /* The resources require Mohan's MOM-patches, see the README */
  "freemem", /* free memory size in KB */
  "ubcmem", /* UBC memory size in KB */
#else
  "mem", /* memory size in KB */
  "resi", /* resident memory size in KB */
#endif
  "nsessions", /* number of sessions in the system */
  "nusers" /* number of users in the system */
  };

/* number of indicies in the res_to_get array */

const int num_resget = sizeof(res_to_get) / sizeof(char *);

void sort_stats(struct batch_status **first);


const int pbs_rm_port = 15003;



int
ShowUsage(void)

  {
  fprintf(stderr, "pestat [ -q ] [ -d ] [ -h <HOSTNAME> ]\n");

  exit(1);
  }  /* END ShowUsage() */






/* usage:  pestat [ -q ] [ -d ] [ -h <HOSTNAME> ] */

int main(

  int   argc,
  char *argv[])

  {

  struct batch_status *bstatus = NULL;

  struct batch_status *jstat = NULL, *job;
  int   con;
  char  *def_server;
  int    errflg = 0;
  char  *errmsg;
  int   i;
  extern char *optarg;
  extern int  optind;
  char  **pa;

  struct batch_status *pbstat;
  int   flag = DOWN;
  char *mom_ans;                /* the answer from mom - getreq() */
  char *node_state;

  struct attrl *pat;

  struct state_map *sm;
  char *value;

  struct attrl *jatt;
  char jstate;
  int jcpu;
  char *on_this_node;
  char joblist[16];
  int nrun, nsus, node_cpu;

  char c;

  struct attrl retattr[] =
    {
    NULL, ATTR_exechost,  NULL, NULL
    };

  retattr[0].next = &retattr[1];
  retattr[1].next = &retattr[2];

  /* get default server, may be changed by -s option */

  def_server = pbs_default();

  if (def_server == NULL)
    def_server = "";

  while ((c = getopt(argc, argv, "dh:qx-:")) != EOF)
    {
    switch (c)
      {

      case 'd':

        DEBUG = TRUE;

        break;

      case 'h':

        /* NYI */

        break;

      case 'q':

        quiet = TRUE;

        break;

      default:

        /* NO-OP */

        ShowUsage();

        break;
      }  /* END switch (c) */
    }

  con = cnt2server(def_server);

  if (con <= 0)
    {
    if (!quiet)
      {
      fprintf(stderr, "%s: cannot connect to server %s, error=%d\n",
              argv[0],
              def_server,
              pbs_errno);
      }

    exit(1);
    }

  bstatus = pbs_statnode(con, "", NULL, NULL);

  if (bstatus == NULL)
    {
    if (pbs_errno != 0)
      {
      if ((errmsg = pbs_geterrmsg(con)) != NULL)
        fprintf(stderr, "%s: %s\n",
                argv[0],
                errmsg);
      else
        fprintf(stderr, "%s: Error %d\n",
                argv[0],
                pbs_errno);

      exit(1);
      }
    else
      {
      fprintf(stderr, "%s: No nodes found\n",
              argv[0]);

      exit(0);
      }
    }

  jstat = pbs_statjob(con, "", NULL, NULL);

  if (jstat == NULL)
    {
    if (pbs_errno)
      {
      if ((errmsg = pbs_geterrmsg(con)) != NULL)
        fprintf(stderr, "%s: %s\n", argv[0], errmsg);
      else
        fprintf(stderr, "%s: Error %d\n", argv[0], pbs_errno);

      exit(1);
      }
    }

  sort_stats(&bstatus);    /* Sort the nodes into name order for readability */

#ifdef TRU64
  printf("        node state  load    pmem ncpu frmem ubcmem usrs jobs   jobids\n");
#else
  printf("        node state  load    pmem ncpu   mem   resi usrs jobs   jobids\n");
#endif

  for (pbstat = bstatus; pbstat; pbstat = pbstat->next)
    {

    node_state = 0;

    for (pat = pbstat->attribs; pat && !node_state; pat = pat->next)
      {
      if (strcmp(pat->name, ATTR_NODE_state) == 0)
        node_state = pat->value;
      }

    if (strstr(node_state, ND_down))
      printf("%12s  down\n", pbstat->name);
    else
      {
      int mom_sd;
      double testd;                 /* used to convert string -> double */
      int testi;                    /* used to convert string -> int */
      char *endp;                   /* used with strtol() */
      double loadave;

      printf("%12s ", pbstat->name);

      for (sm = pstate; sm->rname; sm++)
        {
        if (strcmp(sm->rname, node_state) == 0)
          break;
        }

      if (sm->rname) printf(" %4s", sm->p_rname);
      else           printf("UNKN ");

      if ((mom_sd = openrm(pbstat->name, pbs_rm_port)) < 0)
        {
        printf("No connection to mom on %s\n", pbstat->name);
        continue;
        }
      else

        for (i = 0; i < num_resget; i++) addreq(mom_sd, (char *) res_to_get[i]);

      for (i = 0; i < num_resget && (mom_ans = getreq(mom_sd)) != NULL; i++)
        {

        value = index(mom_ans, '=') + 1;

        if (!strcmp(res_to_get[i], "loadave"))
          {
          loadave = strtod(value, NULL);
          /* Add a flag "*" after the loadave for nodes needing attention */

          if (!strncmp(node_state, "free", 4))   /* Node is "free" */
            {
            if (loadave < 0.5)
              printf("  %s ", value); /* Lightly loaded free node */
            else
              printf("  %s*", value); /* Heavily loaded free node */
            }
          else if (loadave < 0.5 || loadave > 1.5)
            printf("  %s*", value); /* Alert for a low/high load */
          else
            printf("  %s ", value); /* Normal load, or free node */

          }
        else if (!strcmp(res_to_get[i],  "physmem"))
          printf(" %6d", (strtol(value, &endp, 10)) / 1024);

#ifdef TRU64
        else if (!strcmp(res_to_get[i],  "freemem"))
          printf(" %6d", (strtol(value, &endp, 10)) / 1024);
        else if (!strcmp(res_to_get[i],  "ubcmem"))
          printf(" %6d", (strtol(value, &endp, 10)) / 1024);

#else
        else if (!strcmp(res_to_get[i],  "mem"))
          printf(" %6d", (strtol(value, &endp, 10)) / 1024);
        else if (!strcmp(res_to_get[i],  "resi"))
          printf(" %6d", (strtol(value, &endp, 10)) / 1024);

#endif
        else if (!strcmp(res_to_get[i],  "ncpus"))
          printf("  %2d", MIN(atoi(value), 99));
        else if (!strcmp(res_to_get[i],  "nsessions"))
          printf("  %1d", MIN(atoi(value), 9));
        else if (!strcmp(res_to_get[i], "nusers"))
          printf("/%1d ", MIN(atoi(value), 9));

        free(mom_ans);

        fflush(stdout);
        }

      closerm(mom_sd);

      joblist[0] = '\0';
      jcpu = 0;
      nrun = 0;
      nsus = 0;
      node_cpu = 0;

      for (job = jstat; job; job = job->next)
        {
        jatt = job->attribs;
        on_this_node = NULL;

        while (jatt)
          {
          if (!strcmp(jatt->name, ATTR_state))
            jstate = jatt->value[0];
          else if (!strcmp(jatt->name, ATTR_exechost))
            on_this_node = strstr(jatt->value, pbstat->name);
          else if (!strcmp(jatt->name, ATTR_l))
            if (!strcmp(jatt->resource, "cpupercent"))
              jcpu = atoi(jatt->value);

          jatt = jatt->next;
          }

        if (on_this_node)
          {
          nrun += jstate == 'R';
          nsus += jstate == 'S';

          if (joblist[0] != '\0') strcat(joblist, ",");

          if (strlen(joblist) < 15)
            strncat(joblist, strtok(job->name, "."), 15 - strlen(joblist));

          node_cpu += jcpu;
          }
        }

      /* printf(" %1d/%1d  %3.3d %s\n",MIN(nrun,9), MIN(nsus,9), node_cpu, joblist); */
      printf("  %2d   %s\n", MIN(nrun, 99), joblist);
      }
    }
  }



#define NODE_T struct batch_status
#define SIZE(n)  (atoi(n->name+4))
#define SORTLIST sort_stats

void SORTLIST(NODE_T **first)
  {
  /* -----------------------------------------------------------------------
     sorts a "one-way" list of NODE_Ts on component SIZE in ascending order
    
     struct node{
        <type>  SIZE;
          ...
        struct node *next;
     } NODE_T;

    ----------------------------------------------------------------------- */

  NODE_T *done, *i, *j;

  if ((*first)->next != NULL)
    {

    for (done = *first; done->next != NULL;)   /* Sort on size*/
      {

      j = done->next;                  /* extract the next "undone" */
      done->next = j->next;            /* node from the list */

      if (SIZE(j) < SIZE((*first)))      /* Goes first */
        {
        j->next = *first;
        *first = j;
        }
      else                               /* find it splace */
        {
        for (i = *first;
             i != done && SIZE(i->next) <= SIZE(j) ;
             i = i->next){ };

        j->next = i->next;

        i->next = j;

        if (i == done) done = done->next;
        }
      }
    }
  }
