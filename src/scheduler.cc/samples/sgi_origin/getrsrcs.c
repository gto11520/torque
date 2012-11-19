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
/* $Id: getrsrcs.c 2367 2008-09-03 17:17:34Z josh $ */

/*
 *  Obtain resource information from the resource monitor and
 *  job counts from the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <unistd.h>

/* PBS header files */
#include "pbs_error.h"
#include "pbs_ifl.h"
#include "log.h"
#include "resmon.h"
#include "rm.h"

/* Scheduler header files */
#include "toolkit.h"
#include "gblxvars.h"
#include "hpm.h"

#ifndef GETRSRCS_CONNECT_TIME
#define GETRSRCS_CONNECT_TIME 10 /* how long to wait for resource info */
#endif /* ! GETRSRCS_CONNECT_TIME */

Resources *schd_RsrcsList = NULL; /* List of hosts and their resources */

extern char *schd_CmdStr[16];

static void dump_resources(Resources *rsrcs);

/* ARGSUSED */
static void
connect_interrupt(int signo)
  {
  /*
   * Do nothing.  Just accept the signal and allow an alarmed syscall
   * to be interrupted.
   */
  return;
  }

/*
 * Find an entry for the resources for the requested host in the list of
 * existing resources, or create a new one for that host and return it.
 */
Resources *
schd_get_resources(char *exechost)
  {
  char   *id = "schd_get_resources";
  Resources *rptr, *new_rsrcs;
  int     rm;

  char   *response = NULL;
  int     badreply   = 0;
  int     cpus_avail = 0;
  size_t  pmem_avail = 0;

  char    hpm_ctl[64];

  struct sigaction act, oact;

  unsigned int remain; /* Time remaining in any old alarm(). */
  time_t then;  /* When this alarm() was started. */

#ifdef NODEMASK
  Bitfield cpy;
  int     i, j;
#endif /* NODEMASK */

  /*
   * Check for a local copy of the resources being available already.
   * If so, just return a reference to that Resources structure.
   */

  if (schd_RsrcsList != NULL)
    {
    for (rptr = schd_RsrcsList; rptr != NULL; rptr = rptr->next)
      if (strcmp(rptr->exechost, exechost) == 0)
        return (rptr);
    }

  schd_timestamp("get_rsrcs");

  /*
   * No cached resource information for 'exechost'.  Need to query the
   * host for its information.
   */

  if ((new_rsrcs = (Resources *)malloc(sizeof(Resources))) == NULL)
    {
    (void)sprintf(log_buffer, "Unable to alloc space for Resources.");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));

    return (NULL); /* Can't get the information - nowhere to store it. */
    }

  memset((void *)new_rsrcs, 0, sizeof(Resources));

  act.sa_flags = 0;
  act.sa_handler = connect_interrupt;
  sigemptyset(&act.sa_mask);
  remain = 0;
  then = 0;

  /*
   * Set the alarm, and maintain some idea of how long was left on any
   * previously set alarm.
   */

  if (sigaction(SIGALRM, &act, &oact) == 0)
    {
    remain = alarm(GETRSRCS_CONNECT_TIME);
    then = time(NULL);
    }

  if ((rm = openrm(exechost, 0)) == -1)
    {
    (void)sprintf(log_buffer,
                  "Unable to contact resmom@%s (%d)", exechost, pbs_errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

    badreply = 1;
    goto bail;
    }

  /*
   * Turn off full response.  Responses will be received in the order in
   * which they are sent.
   */
  fullresp(0);

  /* Build a list of all the resources about which we want information. */

  addreq(rm, "loadave");

  addreq(rm, "availmem");

  addreq(rm, "physmem");

  addreq(rm, "ncpus");

#ifdef NODEMASK
  addreq(rm, "availmask");

#endif /* NODEMASK */

  if (schd_MANAGE_HPM)
    {
    (void)sprintf(hpm_ctl, HPM_CTL_FORMAT_STR, HPM_CTL_QUERY_STR);
    addreq(rm, hpm_ctl);
    }

  /* Get the values back from the resource monitor, and round up. */

  /* Receive LOADAVE response from resource monitor. */
  response = getreq(rm);

  if (response != NULL)
    {
    new_rsrcs->loadave = atof(response) * schd_FAKE_MACH_MULT;
    (void)free(response);
    }
  else
    {
    (void)sprintf(log_buffer, "bad return from getreq(loadave), %d, %d",
                  pbs_errno, errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    badreply = 1;
    goto bail;
    }

  /* Receive AVAILMEM response from resource monitor. */
  response = getreq(rm);

  if (response != NULL)
    {
    new_rsrcs->freemem = schd_val2byte(response);
    new_rsrcs->freemem *= schd_FAKE_MACH_MULT;
    (void)free(response);
    }
  else
    {
    (void)sprintf(log_buffer, "bad return from getreq(freemem), %d, %d",
                  pbs_errno, errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    badreply = 1;
    goto bail;
    }

  /* Receive PHYSMEM response from resource monitor. */
  response = getreq(rm);

  if (response != NULL)
    {
    pmem_avail = schd_val2byte(response);
    pmem_avail *= schd_FAKE_MACH_MULT;
    (void)free(response);
    }
  else
    {
    (void)sprintf(log_buffer, "bad return from getreq(realmem), %d, %d",
                  pbs_errno, errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    badreply = 1;
    goto bail;
    }

  /* Receive NCPUS response from resource monitor. */
  response = getreq(rm);

  if (response != NULL)
    {
    cpus_avail = atoi(response) * schd_FAKE_MACH_MULT;
    (void)free(response);
    }
  else
    {
    (void)sprintf(log_buffer, "bad return from getreq(ncpus), %d, %d",
                  pbs_errno, errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    badreply = 1;
    goto bail;
    }

#ifdef NODEMASK
  /* Receive available nodes from resource monitor. */
  response = getreq(rm);

  if (response == NULL)
    {
    (void)sprintf(log_buffer, "bad return from getreq(availmask), %d, %d",
                  pbs_errno, errno);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    badreply = 1;
    goto bail;
    }
  else
    {
    if (schd_bits2mask(response, &new_rsrcs->availmask) != 0)
      {
      if (schd_str2mask(response, &new_rsrcs->availmask) != 0)
        {
        (void)sprintf(log_buffer, "can't parse availmask '%s'", response);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
        badreply = 1;
        goto bail;
        }
      }

    (void)free(response);
    }

#endif /* NODEMASK */

  if (schd_MANAGE_HPM)
    {
    /* Receive HPM_CTL response from resource monitor. */
    response = getreq(rm);

    if (response != NULL)
      {
      if (strcmp(response, HPM_CTL_USERMODE_STR) == 0)
        new_rsrcs->flags |= RSRCS_FLAGS_HPM_USER;
      else if (strcmp(response, HPM_CTL_GLOBALMODE_STR) == 0)
        new_rsrcs->flags &= ~RSRCS_FLAGS_HPM_USER;
      else
        {
        (void)sprintf(log_buffer, "bad response '%s' for '%s@%s'",
                      response, hpm_ctl, exechost);
        log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id,
                   log_buffer);
        badreply = 1;
        goto bail;
        }
      }
    else
      {
      (void)sprintf(log_buffer, "bad return from getreq(%s), %d, %d",
                    hpm_ctl, pbs_errno, errno);
      log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
      badreply = 1;
      goto bail;
      }
    }

  /*
   * NOTE: response will be free()'d in bail.  Be sure to explicitly free()
   * response if more getreq() calls are added before the code below.
   */

bail:
  if (response != NULL)
    (void)free(response);

  /* Disconnect from the resource monitor. */
  if (rm >= 0)  /* resmom handle "0" is valid in RPP. */
    closerm(rm);

  /* And unset the alarm and handler. */
  alarm(0);

  sigaction(SIGALRM, &oact, &act);

  /* Reset the old alarm, taking into account how much time has passed. */
  if (remain)
    {
    DBPRT(("%s: old alarm had %d secs remaining, %d elapsed, ", id,
           remain, (time(NULL) - then)));
    /* How much time remains even after the time spent above? */
    remain -= (time(NULL) - then);

    /*
     * Would the previous time have already expired?  If so, schedule
     * an alarm call in 1 second (close enough, hopefully).
     */

    if (remain < 1)
      remain = 1;

    DBPRT(("reset to %d secs\n", remain));

    alarm(remain);
    }

  /*
   * Verify all the data came back as expected; if not, abort this
   * iteration of the scheduler.
   */
  if (badreply)
    {
    (void)sprintf(log_buffer,
                  "Got bad info from mom@%s - aborting sched run", exechost);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));

    free(new_rsrcs);
    return (NULL);
    }

  /* Make a copy of the hostname for the resources struct. */
  new_rsrcs->exechost = schd_strdup(exechost);

  if (new_rsrcs->exechost == NULL)
    {
    (void)sprintf(log_buffer, "Unable to copy exechost %s to rsrcs",
                  exechost);
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    DBPRT(("%s: %s\n", id, log_buffer));

    free(new_rsrcs);
    return (NULL);
    }

  new_rsrcs->nodes_total = NODES_REQD(cpus_avail, pmem_avail);

#ifdef NODEMASK
  /* Copy the availmask schd_FAKE_MACH_MULT times to match avail cpus. */
  BITFIELD_CPY(&cpy, &(new_rsrcs->availmask));

  for (i = 2; i <= schd_FAKE_MACH_MULT; i++)
    {
    for (j = 0; j < (cpus_avail / schd_FAKE_MACH_MULT / 2); j++)
      BITFIELD_SHIFTL(&cpy);

    BITFIELD_SETM(&(new_rsrcs->availmask), &cpy);
    }

#endif /* NODEMASK */

  if (schd_RsrcsList == NULL)
    {
    schd_RsrcsList  = new_rsrcs; /* Start the list. */
    }
  else
    {
    for (rptr = schd_RsrcsList; rptr->next != NULL; rptr = rptr->next)
      /* Find the last element in the list. */ ;

    rptr->next = new_rsrcs;
    }

  /* Next pointer for the tail of the list points to nothing. */
  new_rsrcs->next = NULL;

  return (new_rsrcs);
  }

void
schd_dump_rsrclist(void)
  {
  Resources *rsrcs;

  for (rsrcs = schd_RsrcsList; rsrcs != NULL; rsrcs = rsrcs->next)
    dump_resources(rsrcs);
  }

static void
dump_resources(Resources *rsrcs)
  {
  char   *id = "dump_resources";

  /* Log the system's status */

#if 0
  (void)sprintf(log_buffer, " %d%% usr, %d%% sys, %d%% idl",
                rsrcs->usrtime, rsrcs->systime, rsrcs->idltime);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
#endif /* 0 */

  (void)sprintf(log_buffer,
                "Resources for host %s", rsrcs->exechost);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  (void)sprintf(log_buffer, " :: %-24s = %s", "Memory (free):",
                schd_byte2val(rsrcs->freemem));
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  (void)sprintf(log_buffer, " :: %-24s = %d / %d (%.2f%% utilization)",
                "Nodes allocated:", rsrcs->nodes_alloc, rsrcs->nodes_total,
                (rsrcs->nodes_alloc * 100.0) / rsrcs->nodes_total);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

#ifdef NODEMASK
  (void)sprintf(log_buffer, " :: %-24s = %s", "Nodes configured:",
                schd_format_nodemask(&(rsrcs->availmask), &(rsrcs->availmask)));
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  (void)sprintf(log_buffer, " :: %-24s = %s", "Nodes in use:",
                schd_format_nodemask(&(rsrcs->availmask), &(rsrcs->nodes_used)));
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
#endif /* NODEMASK */

  (void)sprintf(log_buffer, " :: %-24s = %0.2f", "CPU Load average:",
                rsrcs->loadave);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  (void)sprintf(log_buffer, " :: %-24s = %d", "Running jobs:",
                rsrcs->njobs);
  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

  if (schd_MANAGE_HPM)
    {
    (void)sprintf(log_buffer, " :: %-24s = %s mode, %sin use",
                  "HPM Counters:",
                  rsrcs->flags & RSRCS_FLAGS_HPM_USER ? "user" : "global",
                  rsrcs->flags & RSRCS_FLAGS_HPM_IN_USE ? "" : "not ");
    log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
    }
  }
