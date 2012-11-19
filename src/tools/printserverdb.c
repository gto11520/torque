/* print the contents of serverdb */
#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "portability.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"

int no_attributes = 0;

void prt_server_struct(

  struct server *pserver)

  {
  printf("---------------------------------------------------\n");
  printf("numjobs:\t\t%d\n",
         pserver->sv_qs.sv_numjobs);

  printf("numque:\t\t%d\n",
         pserver->sv_qs.sv_numque);

  printf("jobidnumber:\t\t%d\n",
         pserver->sv_qs.sv_jobidnumber);

  printf("savetm:\t\t%ld\n",
         (long)pserver->sv_qs.sv_savetm);

  return;
  }  /* END prt_job_struct() */




#define ENDATTRIBUTES -711

int read_attr(

  int fd)

  {
  int       amt;
  int       i;
  svrattrl *pal;
  svrattrl  tempal;

  i = read(fd, (char *) & tempal, sizeof(tempal));

  if (i != sizeof(tempal))
    {
    fprintf(stderr, "bad read of attribute\n");

    /* FAILURE */

    return(0);
    }

  if (tempal.al_tsize == ENDATTRIBUTES)
    {
    /* FAILURE */

    return(0);
    }

  pal = (svrattrl *)calloc(1, tempal.al_tsize);

  if (pal == NULL)
    {
    fprintf(stderr, "malloc failed\n");

    exit(1);
    }

  *pal = tempal;

  /* read in the actual attribute data */

  amt = pal->al_tsize - sizeof(svrattrl);

  i = read(fd, (char *)pal + sizeof(svrattrl) - 1, amt);

  if (i != amt)
    {
    fprintf(stderr, "short read of attribute\n");

    exit(2);
    }

  pal->al_name = (char *)pal + sizeof(svrattrl);

  if (pal->al_rescln != 0)
    pal->al_resc = pal->al_name + pal->al_nameln;
  else
    pal->al_resc = NULL;

  if (pal->al_valln != 0)
    pal->al_value = pal->al_name + pal->al_nameln + pal->al_rescln;
  else
    pal->al_value = NULL;

  printf("%s",
         pal->al_name);

  if (pal->al_resc != NULL)
    {
    printf(".%s",
           pal->al_resc);
    }

  printf(" = ");

  if (pal->al_value != NULL)
    {
    printf("%s",
           pal->al_value);
    }

  printf("\n");

  free(pal);

  return(1);
  }


void dumpdb(char *file)
  {
  int fp;
  int amt;

  struct server xserver;

  fp = open(file, O_RDONLY, 0);

  if (fp < 0)
    {
    perror("open failed");

    fprintf(stderr, "unable to open file %s\n",
            file);

    exit(1);
    }

  amt = read(fp, &xserver.sv_qs, sizeof(xserver.sv_qs));

  if (amt != sizeof(xserver.sv_qs))
    {
    fprintf(stderr, "Short read of %d bytes, file %s\n",
            amt,
            file);
    }

  /* print out job structure */

  prt_server_struct(&xserver);

  /* now do attributes, one at a time */

  if (no_attributes == 0)
    {
    printf("--attributes--\n");

    while (read_attr(fp));
    }

  close(fp);

  printf("\n");
  }  /* END dumpdb */





int main(

  int argc,
  char *argv[])

  {
  int err = 0;
  int f;
  char defdb[1024];

  extern int optind;

  sprintf(defdb, "%s/%s/%s", PBS_SERVER_HOME, PBS_SVR_PRIVATE, PBS_SERVERDB);

  while ((f = getopt(argc, argv, "a")) != EOF)
    {
    switch (f)
      {

      case 'a':

        no_attributes = 1;

        break;

      default:

        err = 1;

        break;
      }
    }

  if (err)
    {
    fprintf(stderr, "usage: %s [-a] [file]...}\n",
            argv[0]);

    return(1);
    }

  if (optind == argc)
    {
    dumpdb(defdb);
    }
  else
    {
    for (f = optind;f < argc;++f)
      {
      dumpdb(argv[f]);
      }
    }

  return(0);
  }    /* END main() */

/* END printserverdb.c */

