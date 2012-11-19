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
 * decode_DIS_replySvr() - decode a Batch Protocol Reply Structure for Server
 *
 * This routine decodes a batch reply into the form used by server.
 * The only difference between this and the command version is on status
 * replies.  For the server,  the attributes are decoded into a list of
 * server svrattrl structures rather than a commands's attrl.
 *
 *  batch_reply structure defined in libpbs.h, it must be allocated
 * by the caller.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdlib.h>
#include "libpbs.h"
#include "list_link.h"
#include "dis.h"
#include "batch_request.h"

int decode_DIS_replySvr(

  int                 sock,   /* I */
  struct batch_reply *reply)  /* I (modified) */

  {
  int        ct;
  int        i;

  struct brp_select    *psel;

  struct brp_select   **pselx;

  struct brp_status    *pstsvr;
  int        rc = 0;

  /* first decode "header" consisting of protocol type and version */

  i = disrui(sock, &rc);

  if (rc != 0) return rc;

  if (i != PBS_BATCH_PROT_TYPE) return DIS_PROTO;

  i = disrui(sock, &rc);

  if (rc != 0) return rc;

  if (i != PBS_BATCH_PROT_VER) return DIS_PROTO;

  /* next decode code, auxcode and choice (union type identifier) */

  reply->brp_code    = disrsi(sock, &rc);

  if (rc) return rc;

  reply->brp_auxcode = disrsi(sock, &rc);

  if (rc) return rc;

  reply->brp_choice  = disrui(sock, &rc);

  if (rc) return rc;


  switch (reply->brp_choice)
    {

    case BATCH_REPLY_CHOICE_NULL:
      break; /* no more to do */

    case BATCH_REPLY_CHOICE_Queue:

    case BATCH_REPLY_CHOICE_RdytoCom:

    case BATCH_REPLY_CHOICE_Commit:

      if ((rc = disrfst(sock, PBS_MAXSVRJOBID + 1, reply->brp_un.brp_jid)))
        return (rc);

      break;

    case BATCH_REPLY_CHOICE_Select:

      /* have to get count of number of strings first */

      reply->brp_un.brp_select = (struct brp_select *)0;

      pselx = &reply->brp_un.brp_select;

      ct = disrui(sock, &rc);

      if (rc) return rc;

      while (ct--)
        {
        psel = (struct brp_select *)malloc(sizeof(struct brp_select));

        if (psel == 0) return DIS_NOMALLOC;

        psel->brp_next = (struct brp_select *)0;

        psel->brp_jobid[0] = '\0';

        rc = disrfst(sock, PBS_MAXSVRJOBID + 1, psel->brp_jobid);

        if (rc)
          {
          (void)free(psel);
          return rc;
          }

        *pselx = psel;

        pselx = &psel->brp_next;
        }

      break;

    case BATCH_REPLY_CHOICE_Status:

      /* have to get count of number of status objects first */

      CLEAR_HEAD(reply->brp_un.brp_status);
      ct = disrui(sock, &rc);

      if (rc) return rc;

      while (ct--)
        {
        pstsvr = (struct brp_status *)malloc(sizeof(struct brp_status));

        if (pstsvr == 0) return DIS_NOMALLOC;

        CLEAR_LINK(pstsvr->brp_stlink);

        pstsvr->brp_objname[0] = '\0';

        CLEAR_HEAD(pstsvr->brp_attr);

        pstsvr->brp_objtype = disrui(sock, &rc);

        if (rc == 0)
          {
          rc = disrfst(sock, PBS_MAXSVRJOBID + 1,
                       pstsvr->brp_objname);
          }

        if (rc)
          {
          (void)free(pstsvr);
          return rc;
          }

        append_link(&reply->brp_un.brp_status,

                    &pstsvr->brp_stlink, pstsvr);
        rc = decode_DIS_svrattrl(sock, &pstsvr->brp_attr);
        }

      break;

    case BATCH_REPLY_CHOICE_Text:

      /* text reply */

      reply->brp_un.brp_txt.brp_str = disrcs(sock,
                                             &reply->brp_un.brp_txt.brp_txtlen,
                                             &rc);
      break;

    case BATCH_REPLY_CHOICE_Locate:

      /* Locate Job Reply */

      rc = disrfst(sock, PBS_MAXDEST + 1, reply->brp_un.brp_locate);
      break;

    default:
      return -1;
    }

  return rc;
  }



