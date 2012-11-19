#include "license_pbs.h" /* See here for the software license */
/*
 * qstop
 *  The qstop command directs that a destination should stop scheduling
 *  or routing batch jobs.
 *
 * Synopsis:
 *  qstop destination ...
 *
 * Arguments:
 *  destination ...
 *      A list of destinations.  A destination has one of the following
 *      three forms:
 *          queue
 *          @server
 *          queue@server
 *      If queue is specified, the request is to stop the queue at
 *      the default server.  If @server is given, the request is to
 *      stop all queues at the server.  If queue@server is used,
 *      the request is to stop the named queue at the named server.
 *
 * Written by:
 *  Bruce Kelly
 *  National Energy Research Supercomputer Center
 *  Livermore, CA
 *  May, 1993
 */

#include "cmds.h"
#include "net_cache.h"
#include <pbs_config.h>   /* the master config generated by configure */


int exitstatus = 0; /* Exit Status */
static void execute(char *, char *);


int
main(int argc, char **argv)
  {
  /*
   *  This routine sends a Manage request to the batch server specified by
   * the destination.  The STARTED queue attribute is set to {False}.  If the
   * batch request is accepted, the server will stop scheduling or routing
   * requests for the specified queue.
   */

  int dest;  /* Index into the destination array (argv) */
  char *queue; /* Queue name part of destination */
  char *server; /* Server name part of destination */

  if (argc == 1)
    {
    fprintf(stderr, "Usage: qstop [queue][@server] ...\n");
    exit(1);
    }
  else if (argc > 1 && argv[1][0] == '-')
    {
    fprintf(stderr, "Usage: qstop [queue][@server] ...\n");
    exit(1);
    }
  
  initialize_network_info();

  for (dest = 1; dest < argc; dest++)
    if (parse_destination_id(argv[dest], &queue, &server) == 0)
      execute(queue, server);
    else
      {
      fprintf(stderr, "qstop: illegally formed destination: %s\n",
              argv[dest]);
      exitstatus = 1;
      }

  exit(exitstatus);
  }


/*
 * void execute( char *queue, char *server )
 *
 * queue    The name of the queue to disable.
 * server   The name of the server that manages the queue.
 *
 * Returns:
 *  None
 *
 * File Variables:
 *  exitstatus  Set to two if an error occurs.
 */
static void execute(
    
  char *queue,
  char *server)

  {
  int local_errno = 0;
  int ct;         /* Connection to the server */
  int merr;       /* Error return from pbs_manager */
  char *errmsg;   /* Error message from pbs_manager */
  /* The disable request */

  static struct attropl attr =
    {
    NULL, "started", NULL, "FALSE", SET
    };

  if ((ct = cnt2server(server)) > 0)
    {
    merr = pbs_manager_err(ct, MGR_CMD_SET, MGR_OBJ_QUEUE, queue, &attr, NULL, &local_errno);

    if (merr != 0)
      {
      errmsg = pbs_geterrmsg(ct);

      if (errmsg != NULL)
        {
        fprintf(stderr, "qstop: %s ", errmsg);
        }
      else
        {
        fprintf(stderr, "qstop: Error disabling queue: %d - %s\n",
          local_errno,
          pbs_strerror(local_errno));
        }

      if (notNULL(queue))
        fprintf(stderr, "%s", queue);

      if (notNULL(server))
        fprintf(stderr, "@%s", server);

      fprintf(stderr, "\n");

      exitstatus = 2;
      }

    pbs_disconnect(ct);
    }
  else
    {
    fprintf(stderr, "qstop: could not connect to server %s (%d) %s\n",
            server,
            ct * -1,
            pbs_strerror(ct * -1));
    exitstatus = 2;
    }
  }
