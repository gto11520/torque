/* job_qs_upgrade.c - 
 *
 *	The following public functions are provided:
 *		job_qs_upgrade()   - 
 */

#include <pbs_config.h>   /* the master config generated by configure */
 
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"


#define PBS_MAXSVRJOBID_2_1_X	(PBS_MAXSEQNUM + PBS_MAXSERVERNAME + PBS_MAXPORTNUM  + 2 ) /* server job id size */

typedef struct {
    int	    ji_state;		/* internal copy of state */
    int	    ji_substate;	/* job sub-state */
    int	    ji_svrflags;	/* server flags */
    int	    ji_numattr;		/* number of attributes in list */
    int	    ji_ordering;	/* special scheduling ordering */
    int	    ji_priority;	/* internal priority */
    time_t  ji_stime;		/* time job started execution */
    char    ji_jobid[PBS_MAXSVRJOBID_2_1_X + 1];   /* job identifier */
    char    ji_fileprefix[PBS_JOBBASE + 1];  /* job file prefix */
    char    ji_queue[PBS_MAXQUEUENAME + 1];  /* name of current queue */
    char    ji_destin[PBS_MAXROUTEDEST + 1]; /* dest from qmove/route */
    int	    ji_un_type;		/* type of ji_un union */
    union {	/* depends on type of queue currently in */
	struct {	/* if in execution queue .. */
     	    pbs_net_t ji_momaddr;  /* host addr of Server */
	    int	      ji_exitstat; /* job exit status from MOM */
	} ji_exect;
	struct {
	    time_t  ji_quetime;		      /* time entered queue */
	    time_t  ji_rteretry;	      /* route retry time */
	} ji_routet;
	struct {
            pbs_net_t  ji_fromaddr;     /* host job coming from   */
	    int	       ji_fromsock;	/* socket job coming over */
	    int	       ji_scriptsz;	/* script size */
	} ji_newt;
	struct {
     	    pbs_net_t ji_svraddr;  /* host addr of Server */
	    int	      ji_exitstat; /* job exit status from MOM */
	    uid_t     ji_exuid;	   /* execution uid */
	    gid_t     ji_exgid;	   /* execution gid */
	} ji_momt;
    } ji_un;
} ji_qs_2_1_X;
 

/* this function will upgrade a ji_qs struct from the last version to the 
   newest version.  this function needs to know the change in structure, 
   and therefore we should only support upgrades from the previous version 
   
   this version upgrades from 2.1.x to 2.2.0 
   */

int job_qs_upgrade (job *pj, int fds)
  {
  ji_qs_2_1_X qs_old;
  
  if (read(fds, (char*)&qs_old, sizeof(qs_old)) != sizeof(qs_old))
    {
    return -1;
    }
  
  pj->ji_qs.qs_version  = PBS_QS_VERSION;
  pj->ji_qs.ji_state    = qs_old.ji_state;
  pj->ji_qs.ji_substate = qs_old.ji_substate;
  pj->ji_qs.ji_svrflags = qs_old.ji_svrflags;
  pj->ji_qs.ji_numattr  = qs_old.ji_numattr;  
  pj->ji_qs.ji_ordering = qs_old.ji_ordering;
  pj->ji_qs.ji_priority = qs_old.ji_priority;
  pj->ji_qs.ji_stime    = qs_old.ji_stime;
  
  strcpy(pj->ji_qs.ji_jobid, qs_old.ji_jobid);
  strcpy(pj->ji_qs.ji_fileprefix, qs_old.ji_fileprefix);
  strcpy(pj->ji_qs.ji_queue, qs_old.ji_queue);
  strcpy(pj->ji_qs.ji_destin, qs_old.ji_destin);
  
  pj->ji_qs.ji_un_type  = qs_old.ji_un_type;

  /* no change in these unions for 2.1.x -> 2.2.0, just copy the whole thing
     If the union contents did change, I'm not even sure how you would do 
     this without any idea of what is being stored in the union */
  memcpy(&pj->ji_qs.ji_un, &qs_old.ji_un, sizeof(qs_old.ji_un));
      
  return 0;
  } 