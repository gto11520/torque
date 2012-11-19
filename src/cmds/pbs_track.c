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
 * pbs_track - (TORQUE) start tracking a session not spawned through the usual TM interface
 *
 * Authors:
 *      Josh Butikofer
 *      David Jackon
 *      Alan Taufer
 *      Cluster Resources, Inc.
 */

#include <errno.h>
#include "cmds.h"
#include "tm.h"
#include <pbs_config.h>   /* the master config generated by configure */

#define MAXARGS 64

int main(

  int    argc,
  char **argv) /* pbs_track */

  {
  int ArgIndex;
  int NumErrs = 0;

  char *Args[MAXARGS];
  int   aindex = 0;

  int   rc;
  int   pid;

  char tmpJobID[PBS_MAXCLTJOBID];        /* from the command line */

  char JobID[PBS_MAXCLTJOBID];  /* modified job ID for MOM/server consumption */
  char ServerName[MAXSERVERNAME];

  int  DoBackground = 0;

  tmpJobID[0] = '\0';

  /* USAGE: pbs_track [-j <JOBID>] a.out arg1 arg2 ... argN */

#define GETOPT_ARGS "bj:"

  while ((ArgIndex = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    {
    switch (ArgIndex)
      {

      case 'b':

        /* background process */

        DoBackground = 1;

        break;

      case 'j':

        strncpy(tmpJobID, optarg, sizeof(tmpJobID));

        if (tmpJobID[PBS_MAXCLTJOBID-1] != '\0')
          {
          /* truncation occurred! */

          fprintf(stderr, "pbs_track: given job ID too large (> %d)\n",
                  PBS_MAXCLTJOBID);

          exit(-1);
          }

        break;

      default:

        NumErrs++;

        break;
      }
    }

  if ((NumErrs > 0) ||
      (optind >= argc) ||
      (tmpJobID[0] == '\0'))
    {
    static char Usage[] = "USAGE: pbs_track [-j <JOBID>] [-b] a.out arg1 arg2 ... argN\n";
    fprintf(stderr, "%s", Usage);
    exit(2);
    }

  if (get_server(tmpJobID, JobID, ServerName))
    {
    fprintf(stderr, "pbs_track: illegally formed job identifier: '%s'\n", JobID);
    exit(1);
    }

  /* gather a.out and other arguments */

  aindex = 0;

  for (;optind < argc;optind++)
    {
    Args[aindex++] = strdup(argv[optind]);
    printf("Got arg: %s\n",
           Args[aindex-1]);
    }

  Args[aindex] = NULL;

  /* decide if we should fork or not */

  pid = 1;

  if (DoBackground == 1)
    {
    printf("FORKING!\n");

    pid = fork();
    }

  if ((DoBackground == 0) || (pid == 0))
    {
    /* either parent or child, depending on the setting */

    /* call tm_adopt() to start tracking this process */

    rc = tm_adopt(JobID, TM_ADOPT_JOBID, getpid());

    switch (rc)
      {

      case TM_SUCCESS:

        /* success! */

        break;

      case TM_ENOTFOUND:

        fprintf(stderr, "pbs_track: MOM could not find job %s\n",
                JobID);

        break;

      case TM_ESYSTEM:

      case TM_ENOTCONNECTED:

        fprintf(stderr, "pbs_track: error occurred while trying to communication with pbs_mom: %s (%d)\n",
                pbse_to_txt(rc),
                rc);

        break;

      default:

        /* Unexpected error occurred */

        fprintf(stderr, "pbs_track: unexpected error %s (%d) occurred\n",
                pbse_to_txt(rc),
                rc);

        break;
      }  /* END switch(rc) */

    if (rc != TM_SUCCESS)
      {
      exit(-1);
      }

    /* do the exec */

    execvp(Args[0], Args);
    }  /* END if ((DoBackground == 0) || (pid == 0)) */
  else if (pid > 0)
    {
    /* parent*/

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);
    }
  else if (pid < 0)
    {
    fprintf(stderr, "pbs_track: could not fork (%d:%s)\n",
            errno,
            strerror(errno));
    }

  exit(0);
  }  /* END main() */
