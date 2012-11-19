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
 * que_atrall_def is the array of pbs_attribute defs common to all queue types.
 * que_atrexec_def  is the array of pbs_attribute defs special to execution queues.
 * qu_attr_rt  is the array of pbs_attribute defs special to routing queues.
 *
 * Each legal queue pbs_attribute is defined here.
 */

#include <pbs_config.h>		/* the master config generated by configure */

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "svrfunc.h"

extern int set_null (pbs_attribute * patr, pbs_attribute * new, enum batch_op op);
extern int disallowed_types_chk (pbs_attribute * pattr, void *pobject,
				 int actmode);

/* array of allowable strings in queue pbs_attribute disallowed_types */
char *array_disallowed_types[] = {
  Q_DT_batch,
  Q_DT_interactive,
  Q_DT_rerunable,
  Q_DT_nonrerunable,
  Q_DT_fault_tolerant,
  Q_DT_fault_intolerant,
  Q_DT_job_array,
  "_END_"			/* must be last string */
};


/*
 * The entries for each pbs_attribute are (see attribute.h):
 * name,
 * decode function,
 * encode function,
 * set function,
 * compare function,
 * free value space function,
 * action function,
 * access permission flags,
 * value type
 */

/* NOTE:  que_attr_def[] should be ordered with QA_ATR_* enum */

/**
 * NOTE:  to add new queue pbs_attribute:
 * 1) add ATTR_* #define in qmgr_que_public.h
 * 2) if pbs_attribute is to be publicly viewable/modifiable, add to XXX array
 *    in qmgr_que_public.h
 * 3) add QA_ATR_* to enum queueattr in src/include/queue.h
 * ...
 */

/* for all queues */

attribute_def que_attr_def[] =
  {

  /* QA_ATR_QType */
    { ATTR_qtype,  /* "queue_type" - type of queue */
    decode_str,
    encode_str,
    set_str,
    comp_str,
    free_str,
    set_queue_type,
    NO_USER_SET,
    ATR_TYPE_STR,
    PARENT_TYPE_QUE_ALL
    },
  /* QA_ATR_Priority */  /* priority of queue relative to others */
  { ATTR_p,   /* "priority" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },

  /* QA_ATR_HostList */            /* HostList */
  {   ATTR_hostlist,           /* "hostlist" */
      decode_arst,
      encode_arst,
      set_hostacl,
      comp_arst,
      free_arst,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_ACL,
      PARENT_TYPE_QUE_ALL
  },

  /* QA_ATR_Rerunnable */    /* rerunnable */
  {   ATTR_rerunnable,   /* "rerunnable" */
      decode_b,
      encode_b,
      set_b,
      comp_b,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_QUE_ALL
  },

  /* QA_ATR_MaxJobs */  /* max number of jobs allowed in queue */
  { ATTR_maxque,  /* "max_queuable" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_MaxUserJobs */ /* max number of jobs per user allowed in queue */
  { ATTR_maxuserque, /* max_user_queuable */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QS_ATR_TotalJobs */  /* current number of jobs in queue */
  { ATTR_total,  /* "total_jobs" */
    decode_null,
    encode_l,
    set_null,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_JobsByState */ /* current number of jobs in queue by state */
  { ATTR_count,  /* "state_count" */
    decode_null,  /* note-use fixed memory in queue struct    */
    encode_str,
    set_null,
    comp_str,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_STR,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_MaxReport */  /* max number of jobs reported for truncated output */
  { ATTR_maxreport,  /* "max_report" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_MaxRun */             /* max number of jobs allowed to run */
  {   ATTR_maxrun,            /* "max_running" */
      decode_l,
      encode_l,
      set_l,
      comp_l,
      free_null,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_LONG,
      PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclHostEnabled */ /* Host ACL to be used */
  { ATTR_aclhten,  /* "acl_host_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclHost */  /* Host Access Control List */
  { ATTR_aclhost,  /* "acl_hosts" */
    decode_arst,
    encode_arst,
    set_hostacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclUserEnabled */ /* User ACL to be used */
  { ATTR_acluren,  /* "acl_user_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclUsers */  /* User Acess Control List */
  { ATTR_acluser,  /* "acl_users" */
    decode_arst,
    encode_arst,
    set_uacl,
    comp_arst,
    free_arst,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_FromRouteOnly */ /* Jobs can only enter from a routing queue */
  { ATTR_fromroute,  /* "from_route_only" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_ResourceMax */
  { ATTR_rescmax,  /* "resources_max" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_ResourceMin */
  { ATTR_rescmin,  /* "resources_min" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_ResourceDefault */
  { ATTR_rescdflt,  /* "resources_default" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_QUE_ALL
  },

  /* QA_ATR_AclGroupEnabled */ /* Group ACL to be used */
  { ATTR_aclgren,  /* "acl_group_enable" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclGroup */  /* Group Access Control List */
  { ATTR_aclgroup,  /* "acl_group_list" */
    decode_arst,
    encode_arst,
    set_arst,
    comp_arst,
    free_arst,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_ACL,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclLogic */
  {  ATTR_acllogic,          /* "acl_logic_or" */
     decode_b,
     encode_b,
     set_b,
     comp_b,
     free_null,
     NULL_FUNC,
     NO_USER_SET,
     ATR_TYPE_LONG,
     PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_AclGroupSloppy */
  {  ATTR_aclgrpslpy,          /* "acl_group_sloppy" */
     decode_b,
     encode_b,
     set_b,
     comp_b,
     free_null,
     NULL_FUNC,
     NO_USER_SET,
     ATR_TYPE_LONG,
     PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_MTime */
  { ATTR_mtime,  /* "mtime" */
    decode_l,
    encode_l,
    set_null,
    comp_l,
    free_null,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_DisallowedTypes */
  {   ATTR_disallowedtypes,   /* "disallowed_types" */
      decode_arst,
      encode_arst,
      set_arst,
      comp_arst,
      free_arst,
      disallowed_types_chk,
      NO_USER_SET,
      ATR_TYPE_ACL,
      PARENT_TYPE_QUE_ALL
  },

  /* for execution queues only */


  /* QE_ATR_checkpoint_dir */
  {   ATTR_checkpoint_dir,   /* "checkpoint_dir" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_checkpoint_min */
  { ATTR_checkpoint_min,  /* "checkpoint_min" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_checkpoint_defaults */
  {   ATTR_checkpoint_defaults,   /* "checkpoint_defaults" */
      decode_str,
      encode_str,
      set_str,
      comp_str,
      free_str,
      NULL_FUNC,
      NO_USER_SET,
      ATR_TYPE_STR,
      PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_RendezvousRetry */ /* Number times to retry sync of jobs */
  { "rendezvous_retry",
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_ReservedExpedite */
  { "reserved_expedite",
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_ReservedSync */
  { "reserved_sync",
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_ResourceAvail */
  { "resources_available",
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_RESC,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_ResourceAssn */
  { ATTR_rescassn,  /* "resources_assigned" */
    decode_resc,
    encode_resc,
    set_resc,
    comp_resc,
    free_resc,
    NULL_FUNC,
    READ_ONLY,
    ATR_TYPE_RESC,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_KillDelay */
  { ATTR_killdelay,  /* "kill_delay" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_MaxUserRun */
  { ATTR_maxuserrun, /* "max_user_run" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_MaxGrpRun */
  { ATTR_maxgrprun,  /* "max_group_run" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QE_ATR_KeepCompleted */
  { ATTR_keepcompleted,  /* "keep_completed" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* QR_ATR_is_transit */
  { ATTR_is_transit,
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_EXC
  },
  /* for routing queues */

  /* QR_ATR_RouteDestin */
  { ATTR_routedest,  /* "route_destinations" */
    decode_arst,
    encode_arst,
    set_arst,
    comp_arst,
    free_arst,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_ARST,
    PARENT_TYPE_QUE_RTE
  },
  /* QR_ATR_AltRouter */
  { ATTR_altrouter,  /* "alt_router" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_RTE
  },
  /* QR_ATR_RouteHeld */
  { ATTR_routeheld,  /* "route_held_jobs" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_RTE
  },
  /* QR_ATR_RouteWaiting */
  { ATTR_routewait,  /* "route_waiting_jobs" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_RTE
  },
  /* QR_ATR_RouteRetryTime */
  { ATTR_routeretry, /* "route_retry_time" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_RTE
  },
  /* QR_ATR_RouteLifeTime  */
  { ATTR_routelife,  /* "route_lifetime" */
    decode_l,
    encode_l,
    set_l,
    comp_l,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_RTE
  },
  /* site supplied pbs_attribute definitions, if any, see site_que_attr_*.h */
#include "site_que_attr_def.h"

  /* QA_ATR_Enabled */  /* Queue enabled - jobs can be enqueued */
  { ATTR_enable,  /* "enabled" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    check_que_enable,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  },
  /* QA_ATR_Started */  /* Queue enabled - jobs can be started */
  { ATTR_start,  /* "started" */
    decode_b,
    encode_b,
    set_b,
    comp_b,
    free_null,
    NULL_FUNC,
    NO_USER_SET,
    ATR_TYPE_LONG,
    PARENT_TYPE_QUE_ALL
  }
  };
