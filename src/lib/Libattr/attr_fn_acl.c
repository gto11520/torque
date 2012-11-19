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
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <grp.h>
#include <sys/types.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains general functions for attributes of type
 *      User/Group/Hosts Acess Control List.
 *
 * The following functions should be used for the 3 types of ACLs:
 *
 *  User ACL  Group ACL  Host ACL
 * (+ mgrs + ops)
 * ---------------------------------------------
 *  decode_arst decode_arst decode_arst
 *  encode_arst encode_arst encode_arst
 * set_uacl set_arst set_hostacl
 * comp_arst comp_arst comp_arst
 * free_arst free_arst free_arst
 *
 * The "encoded" or external form of the value is a string with the orginial
 * strings separated by commas (or new-lines) and terminated by a null.
 *
 * The "decoded" form is a set of strings pointed to by an array_strings struct
 *
 * These forms are identical to ATR_TYPE_ARST, and in fact encode_arst(),
 * comp_arst(), and free_arst() are used for those functions.
 *
 * set_ugacl() is different because of the special  sorting required.
 */

/* External Functions called */

/* Private Functions */

static int hacl_match(const char *can, const char *master);
static int uhacl_match(const char *can, const char *master);
static int user_match(const char *can, const char *master);
static int gid_match(const char *can, const char *master);
static int host_order(char *old, char *new);
static int user_order(char *old, char *new);
static int set_allacl(attribute *, attribute *, enum batch_op,
                          int (*order_func)());

/* for all decode_*acl() - use decode_arst() */
/* for all encode_*acl() - use encode_arst() */

/*
 * set_uacl - set value of one User ACL attribute to another
 * with special sorting.
 *
 * A=B --> set of strings in A replaced by set of strings in B
 * A+B --> set of strings in B appended to set of strings in A
 * A-B --> any string in B found is A is removed from A
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int
set_uacl(struct attribute *attr, struct attribute *new, enum batch_op op)
  {

  return (set_allacl(attr, new, op, user_order));
  }

/*
 * set_hostacl - set value of one Host ACL attribute to another
 * with special sorting.
 *
 * A=B --> set of strings in A replaced by set of strings in B
 * A+B --> set of strings in B appended to set of strings in A
 * A-B --> any string in B found is A is removed from A
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int set_hostacl(

  struct attribute *attr,
  struct attribute *new,
  enum batch_op     op)

  {
  return(set_allacl(attr, new, op, host_order));
  }





/*
 * acl_check - check a name:
 *  user or [user@]full_host_name
 *  group_name
 *  full_host_name
 * against the entries in an access control list.
 * Match is done by calling the approprate comparison function
 * with the name and each string from the list in turn.
 *
 * Returns: 1 if access is allowed;  0 if not allowed
 */

int acl_check(

  attribute *pattr,
  char      *name,  /* I (optional) */
  int      type)

  {
  int        i;
#ifdef HOST_ACL_DEFAULT_ALL
  int        default_rtn = 1;
#else /* HOST_ACL_DEFAULT_ALL */
  int        default_rtn = 0;
#endif /* HOST_ACL_DEFAULT_ALL */

  struct array_strings *pas;
  char       *pstr;
  int (*match_func)(const char *, const char *);

  extern char server_host[];

  switch (type)
    {

    case ACL_Host:

      match_func = hacl_match;
      break;

    case ACL_User:

      match_func = user_match;
      break;

    case ACL_Gid:

      match_func = gid_match;
      break;

    case ACL_User_Host:
      match_func = uhacl_match;
      break;

    case ACL_Group:

    default:

      match_func = (int (*)())strcmp;

      break;
    }

  if (!(pattr->at_flags & ATR_VFLAG_SET) ||
      ((pas = pattr->at_val.at_arst) == NULL) || (pas->as_usedptr == 0))
    {
#ifdef HOST_ACL_DEFAULT_ALL
    /* no list, default to everybody is allowed */

    return(1);
#else

    if (type != ACL_Host)
      {
      /* FAILURE - deny */

      return(0);
      }

    /* if there is no list set, allow only from my host */

    return(!hacl_match(name, server_host));

#endif
    }

  for (i = 0; i < pas->as_usedptr;i++)
    {
    pstr = pas->as_string[i];

    if ((*pstr == '+') || (*pstr == '-'))
      {
      if (*(pstr + 1) == '\0') /* "+" or "-" sets default */
        {
        if (*pstr == '+')
          default_rtn = 1;  /* allow by default */
        else
          default_rtn = 0;  /* deny by default */
        }

      pstr++;  /* skip over +/- if present */
      }

    if ((name != NULL) && !match_func(name, pstr))
      {
      /* acl matches */

      if (*pas->as_string[i] == '-')
        {
        /* deny */

        return(0);
        }

      /* allow */

      return(1);
      }
    }    /* END for (i) */

  return(default_rtn);
  }  /* END acl_check() */





/*
 * chk_dup_acl - check for duplicate in list (array_strings)
 * Return 0 if no duplicate, 1 if duplicate within the new list or
 * between the new and old list.
 */

static int chk_dup_acl(

  struct array_strings *old,
  struct array_strings *new)

  {
  int i;
  int j;

  for (i = 0;i < new->as_usedptr;++i)
    {
    /* first check against self */

    for (j = 0; j < new->as_usedptr; ++j)
      {

      if (i != j)
        {
        if (strcmp(new->as_string[i], new->as_string[j]) == 0)
          return 1;
        }
      }

    /* next check new against existing (old) strings */

    for (j = 0; j < old->as_usedptr; ++j)
      {

      if (strcmp(new->as_string[i], old->as_string[j]) == 0)
        return 1;
      }
    }

  return 0;
  }

/*
 * set_allacl - general set function for all types of acls
 * This function is private to this file.  It is called
 * by the public set function which is specific to the
 * ACL type.  The public function passes an extra
 * parameter which indicates the ACL type.
 */

static int set_allacl(struct attribute *attr, struct attribute *new, enum batch_op op, int (*order_func)(char *, char *))
  {
  int  i;
  int  j;
  int   k;
  int  nsize;
  int    need;
  long  offset;
  char *pc;
  char *where;
  int  used;

  struct array_strings *tmppas;

  struct array_strings *pas;

  struct array_strings *newpas;

  assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

  pas = attr->at_val.at_arst; /* array of strings control struct */
  newpas = new->at_val.at_arst; /* array of strings control struct */

  if (!newpas)
    return (PBSE_INTERNAL);

  if (!pas)
    {

    /* no array_strings control structure, make one */

    i = newpas->as_npointers;
    pas = (struct array_strings *)malloc((i - 1) * sizeof(char *) +
                                         sizeof(struct array_strings));

    if (!pas)
      return (PBSE_SYSTEM);

    pas->as_npointers = i;

    pas->as_usedptr = 0;

    pas->as_bufsize = 0;

    pas->as_buf     = (char *)0;

    pas->as_next   = (char *)0;

    attr->at_val.at_arst = pas;
    }

  /*
   * At this point we know we have a array_strings struct initialized
   */

  switch (op)
    {

    case SET:

      /*
       * Replace old array of strings with new array, this is
       * simply done by deleting old strings and adding the
       * new strings one at a time via Incr
       */

      for (i = 0; i < pas->as_usedptr; i++)
        pas->as_string[i] = (char *)0; /* clear all pointers */

      pas->as_usedptr = 0;

      pas->as_next   = pas->as_buf;

      if (newpas->as_usedptr == 0)
        break; /* none to set */

      nsize = newpas->as_next - newpas->as_buf; /* space needed */

      if (nsize > pas->as_bufsize)     /* new won't fit */
        {
        if (pas->as_buf)
          free(pas->as_buf);

        nsize += nsize / 2;  /* alloc extra space */

        if (!(pas->as_buf = malloc((size_t)nsize)))
          {
          pas->as_bufsize = 0;
          return (PBSE_SYSTEM);
          }

        memset(pas->as_buf, 0, nsize);
        pas->as_bufsize = nsize;

        }
      else     /* str does fit, clear buf */
        {
        (void)memset(pas->as_buf, 0, pas->as_bufsize);
        }

      pas->as_next = pas->as_buf;

      /* No break, "Set" falls into "Incr" to add strings */

    case INCR_OLD:
    case INCR:

      /* check for duplicates within new and between new and old  */

      if (chk_dup_acl(pas, newpas))
        return (PBSE_DUPLIST);

      nsize = newpas->as_next - newpas->as_buf;   /* space needed */

      used = pas->as_next - pas->as_buf;     /* space used   */

      if (nsize > (pas->as_bufsize - used))
        {

        /* need to make more room for sub-strings */

        need = pas->as_bufsize + 2 * nsize;  /* alloc new buf */

        if (pas->as_buf)
          pc = realloc(pas->as_buf, (size_t)need);
        else
          pc = malloc((size_t)need);

        if (pc == (char *)0)
          return (PBSE_SYSTEM);

        offset = pc - pas->as_buf;

        pas->as_buf   = pc;

        pas->as_next = pc + used;

        pas->as_bufsize = need;

        for (j = 0; j < pas->as_usedptr; j++) /* adjust points */
          pas->as_string[j] += offset;
        }

      j = pas->as_usedptr + newpas->as_usedptr;

      if (j > pas->as_npointers)
        {

        /* need more pointers */

        j = 3 * j / 2;  /* allocate extra     */
        need = (int)sizeof(struct array_strings) + (j - 1) * sizeof(char *);
        tmppas = (struct array_strings *)realloc((char *)pas, (size_t)need);

        if (tmppas == (struct array_strings *)0)
          return (PBSE_SYSTEM);

        tmppas->as_npointers = j;

        pas = tmppas;

        attr->at_val.at_arst = pas;
        }

      /* now put in new strings in special ugacl sorted order */

      for (i = 0; i < newpas->as_usedptr; i++)
        {
        for (j = 0; j < pas->as_usedptr; j++)
          {
          if (order_func(pas->as_string[j], newpas->as_string[i]) > 0)
            break;
          }

        /* push up rest of old strings to make room for new */

        offset = strlen(newpas->as_string[i]) + 1;

        if (j < pas->as_usedptr)
          {
          where  = pas->as_string[j];  /* where to put in new */

          pc = pas->as_next - 1;

          while (pc >= pas->as_string[j])   /* shift data up */
            {
            *(pc + offset) = *pc;
            pc--;
            }

          for (k = pas->as_usedptr; k > j; k--)
            /* re adjust pointrs */
            pas->as_string[k] = pas->as_string[k-1] + offset;
          }
        else
          {
          where = pas->as_next;
          }

        (void)strcpy(where, newpas->as_string[i]);
        pas->as_string[j] = where;
        pas->as_usedptr++;
        pas->as_next += offset;
        }

      break;

    case DECR: /* decrement (remove) string from array */

      for (j = 0; j < newpas->as_usedptr; j++)
        {
        for (i = 0; i < pas->as_usedptr; i++)
          {
          if (!strcmp(pas->as_string[i], newpas->as_string[j]))
            {
            /* compact buffer */
            nsize = (int)strlen(pas->as_string[i]) + 1;
            pc = pas->as_string[i] + nsize;
            need = pas->as_next - pc;
            memmove(pas->as_string[i], pc, (size_t)need);
            pas->as_next -= nsize;
            /* compact pointers */

            for (++i; i < pas->as_npointers; i++)
              pas->as_string[i-1] = pas->as_string[i] - nsize;

            pas->as_string[i-1] = (char *)0;

            pas->as_usedptr--;

            break;
            }
          }
        }

      break;

    default:
      return (PBSE_INTERNAL);
    }

  attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return (0);
  }


/*
 * user_match - User order match
 * Match two strings by user, then from the tail end first
 *
 * Canidate string (first parameter) is a single user@host string.
 *
 * Master string (2nd parameter) is an entry from a user/group acl.
 * It should have a leading + or - which is ignored.  Next is the user name
 * which is compared first.  If the user name matches, then the host name is
 * checked.  The host name may be a wild carded or null (including no '@').
 * If the hostname is null, it is treated the same as "@*", a fully wild
 * carded hostname that matches anything.
 *
 * Returns 0 if strings match,  1 if not   - to match strcmp()
 */

static int
user_match(const char *can, const char *master)
  {

  /* match user name first */

  do
    {
    if (*master != *can)
      return (1); /* doesn't match */

    master++;

    can++;
    }
  while ((*master != '@') && (*master != '\0'));

  if (*master == '\0')
    {
    /* if full match or if master has no host (=wildcard) */
    if ((*can == '\0') || (*can == '@'))
      return (0);
    else
      return (1);
    }
  else if (*can != '@')
    return (1);

  /* ok, now compare host/domain name working backwards     */
  /* if hit wild card in master ok to stop and report match */

  return (hacl_match(can + 1, master + 1));
  }





/*
 * user_order - user order compare
 * compare:
 * (1) the user names, if they are equal, then
 * (2) two host entrys from the tail end first
 *
 * Returns -1 if entry s1 sorts before s2
 *     0 if equal
 *     1 if s1 sorts after s2
 */

static int user_order(

  char *s1,
  char *s2)

  {
  int  d;

  /* skip over the + or - prefix */

  if ((*s1 == '+') || (*s1 == '-'))
    s1++;

  if ((*s2 == '+') || (*s2 == '-'))
    s2++;

  /* compare user names first, stop with '@' */

  while (1)
    {
    if ((d = (int) * s1 - (int) * s2) != 0)
      return (d);

    if ((*s1 == '@') || (*s1 == '\0'))
      return (host_order(s1 + 1, s2 + 1)); /* order host names */

    s1++;

    s2++;
    }

  return(0);
  }  /* END user_order() */


/*
 * group acl match - match 2 groups by gid
 */
static int gid_match(const char *group1, const char *group2)
  {

  struct group *pgrp;
  gid_t gid1, gid2;

  if (!strcmp(group1, group2))
    {
    return(0); /* match */
    }

  pgrp = getgrnam(group1);

  if (pgrp == NULL)
    return(1);

  gid1 = pgrp->gr_gid;

  pgrp = getgrnam(group2);

  if (pgrp == NULL)
    return(1);

  gid2 = pgrp->gr_gid;

  return (!(gid1 == gid2));
  }




/*
 * checks if the range portion of a hostname matches the range in an acl
 * if they match, moves the pointers past this portion
 * should receive things like pm_ptr->->[0-4] and pc_ptr->->3
 *
 * @param pm_ptr - pointer to the pointer to the acl
 * @param pc_ptr - pointer to the pointer to the hostname we're checking
 * @return 0 if match, 1 otherwise
 */
int acl_check_range(

  const char **pm_ptr, /* I/O */
  const char **pc_ptr) /* I/O */

  {
  const char *pm = *pm_ptr;
  const char *pc = *pc_ptr;

  int   low;
  int   high;
  int   num;

  if (*pm == '[')
    pm++;
  
  low = atoi(pm);

  /* find the dash */
  while ((pm != NULL) &&
         (*pm != '-'))
    pm++;

  /* move past the dash */
  pm++;

  high = atoi(pm);

  /* find the closing bracket */
  while ((pm != NULL) && 
         (*pm != ']'))
    pm++;

  /* move past the ] */
  pm++;

  num = atoi(pc);

  if ((num < low) ||
      (num > high))
    return(1);

  /* it matches, now update the pointers */

  /* move pc past the digits */
  while (isdigit(*pc))
    pc++;

  *pc_ptr = pc;
  *pm_ptr = pm;

  return(0);
  } /* END acl_check_range() */




int acl_wildcard_check(

  const char **pm_ptr,     /* I/O */
  const char **pc_ptr,     /* I/O */
  const char  *master_end, /* I */
  const char  *can_end)    /* I */

  {
  const char *pm = *pm_ptr;
  const char *pc = *pc_ptr;

  if (*pm != '*')
    return(1);

  pm++;

  /* we only have to do more if this isn't the end */
  if (pm < master_end)
    {
    /* search through the "can" string to find a match for the rest of
     * the acl */
    while ((strcasecmp(pm,pc) != 0) &&
           (pc < can_end))
      {
      pc++;
      }

    if (pc >= can_end)
      return(1);
    }

  /* we're matching */
  *pm_ptr = master_end;
  *pc_ptr = can_end;

  return(0);
  } /* END acl_wildcard_check() */





/*
 * host acl order match - match two strings from the tail end first
 *
 * Master string (2nd parameter) is an entry from a host acl.  It may have a
 * leading + or - which is ignored.  It may also have an '*' as a leading
 * name segment to be a wild card - match anything.
 *
 * Strings match if identical, or if match up to leading '*' on master which
 * like a wild card, matches any prefix string on canidate domain name
 *
 * Returns 0 if strings match,  1 if not   - to match strcmp()
 */

static int hacl_match(

  const char *can,
  const char *master)

  {
  const char *pc;
  const char *pm;
  const char *can_end;
  const char *master_end;

  if ((can == NULL) || (!strcmp(can, "LOCAL")))
    {
    return(0);
    }

  pc = can;
  pm = master;

  can_end    = can + strlen(can);
  master_end = master + strlen(master);

  while ((pc < can_end) && 
         (pm < master_end))
    {
    switch (*pm)
      {
      case '[':

        if (acl_check_range(&pm,&pc) != 0)
          return(1);

        break;

      case '*':

        if (acl_wildcard_check(&pm,&pc,master_end,can_end) != 0)
          return(1);

        break;

      default:
        if (tolower(*pc) != tolower(*pm))
          return(1);

        /* only advance pointers here, other functions advance properly */
        pc++;
        pm++;

        break;
      }
    }

  /* make sure both strings have terminated or the master has a wildcard */
  if (pc < can_end)
    return(1);
  else if (pm < master_end)
    {
    if ((*pm != '*') ||
        (pm+1 < master_end))
      return(1);
    }

  /* if we haven't failed by now, we're golden */
  return (0);
  }


int match_strings(const char *can, const char *master)
  {

    const char *can_end;
    const char *master_end;

    can_end    = can + strlen(can);
    master_end = master + strlen(master);

  /* see if the strings match up */
  while ((can < can_end) && 
         (master < master_end) && 
         (*can != 0) && (*master != 0))
    {
    switch (*master)
      {
      case '[':

        if (acl_check_range(&master,&can) != 0)
          return(1);

        break;

      case '*':

        if (acl_wildcard_check(&master,&can,master_end,can_end) != 0)
          return(1);

        break;

      default:
        if (tolower(*can) != tolower(*master))
          return(1);

        /* only advance pointers here, other functions advance properly */
        can++;
        master++;

        break;
      }
    }

  /* make sure both strings have terminated or the master has a wildcard */
  if (can < can_end)
    return(1);
  else if (master < master_end)
    {
    if ((*master != '*') ||
        (master+1 < master_end))
      return(1);
    }

  return(0);
  }

/*
 * user/host acl order match - match two strings from the tail end first
 * 
 * uhacl_match will accept names is a <user>@<host> format. The @ character
 * is used as a delimeter to separate the user from the host. A string match
 * is then done on each half of the incoming string. If both halves of can
 * match both halves of master a 0 is returned otherwise a 1.
 *
 *
 * Master string (2nd parameter) is an entry from a host acl.  It may have a
 * leading + or - which is ignored.  It may also have an '*' as a leading
 * name segment to be a wild card - match anything.
 *
 * Strings match if identical, or if match up to leading '*' on master which
 * like a wild card, matches any prefix string on canidate domain name
 *
 * Returns 0 if strings match,  1 if not   - to match strcmp()
 */

static int uhacl_match(

  const char *can,
  const char *master)

  {
  const char *pchost;
  const char *pmhost;
  const char *pcuser;
  const char *pmuser;
  const char *ptr;

  if ((can == NULL) || (!strcmp(can, "LOCAL")))
    {
    return(0);
    }

  /* separate the user and the host portions */
  ptr = strchr(can, '@');
  if(ptr == NULL)
    {
    /* we are expecting the format <user>@<host>. No @ was found */
    return(1);
    }

  pchost = ptr;
  pchost++;
  if(*pchost == 0)
    {
    /* no host given. Fail*/
    return(1);
    }

  /* set pcuser to can and then null terminate user by
     setting ptr to 0 */
  pcuser = can;
  ptr = NULL;

  /* next separate the user and host for the master */
  ptr = strchr(master, '@');
  if(ptr == NULL)
    {
    /* we are expecting the format <user>@<host>. No @ was found */
    return(1);
    }

  pmhost = ptr;
  pmhost++;
  if(*pmhost == 0)
    {
    /* no host given. Fail */
    return(1);
    }
  
  pmuser = master;
  ptr = NULL;

  if(match_strings(pcuser, pmuser))
    {
    return(1);
    }

  if(match_strings(pchost, pmhost))
    {
      return(1);
    }



  /* if we haven't failed by now, we're golden */
  return (0);
  }





/*
 * host reverse order compare - compare two host entrys from the tail end first
 * domain name segment at at time.
 *
 * Returns -1 if entry s1 sorts before s2
 *     0 if equal
 *     1 if s1 sorts after s2
 */

static int host_order(

  char *s1,
  char *s2)

  {
  int  d;
  char *p1;
  char *p2;

  if ((*s1 == '+') || (*s1 == '-'))
    s1++;

  if ((*s2 == '+') || (*s2 == '-'))
    s2++;

  p1 = s1 + strlen(s1) - 1;

  p2 = s2 + strlen(s2) - 1;

  while (1)
    {
    d = (int) * p2 - (int) * p1;

    if ((p1 > s1) && (p2 > s2))
      {
      if (d != 0)
        return (d);
      else
        {
        p1--;
        p2--;
        }
      }
    else if ((p1 == s1) && (p2 == s2))
      {
      if (*p1 == '*')
        return (1);
      else if (*p2 == '*')
        return (-1);
      else
        return (d);
      }
    else  if (p1 == s1)
      {
      return (1);
      }
    else
      {
      return (-1);
      }
    }

  return(0);
  }

