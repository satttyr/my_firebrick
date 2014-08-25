/******************************************************************************
The software provided here is released by the National
Institute of Standards and Technology (NIST), an agency of
the U.S. Department of Commerce, Gaithersburg MD 20899,
USA.  The software bears no warranty, either expressed or
implied. NIST does not assume legal liability nor
responsibility for a User's use of the software or the
results of such use.

Please note that within the United States, copyright
protection, under Section 105 of the United States Code,
Title 17, is not available for any work of the United
States Government and/or for any works created by United
States Government employees. User acknowledges that this
software contains work which was created by NIST employees
and is therefore in the public domain and not subject to
copyright.  The User may use, distribute, or incorporate
this software provided the User acknowledges this via an
explicit acknowledgment of NIST-related contributions to
the User's work. User also agrees to acknowledge, via an
explicit acknowledgment, that any modifications or
alterations have been made to this software before
redistribution.
******************************************************************************/
static char *SCCS_ID[] = {"@(#) logsetup.c Linux Version 1.2 Created 02/18/05 at 08:49:40",
				__DATE__,__TIME__};
/***** Author: Dr. James R. Lyle, NIST/SDCT/SQG ****/
/* Modified by Kelsey Rider, NIST/SDCT Sept 2004 */
# include <stdio.h>
# include "zbios.h"
# include <time.h>
# include <string.h>
# include <malloc.h>
# include <fcntl.h>
# include <errno.h>
/*****************************************************************
 Log basic test case information
*****************************************************************/

/*****************************************************************
Print instructions about running cmd and options
*****************************************************************/
void print_help(char *p)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s disk host operator OS\n",p);
}

main (int np, char **p)
{
	static time_t	from; /* start time */
	FILE		*log; /* the log file */
	int		x;

	time(&from);
	printf ("\n%s\n %s\ncompiled at %s on %s\n", p[0],SCCS_ID[0],
		__TIME__,__DATE__);

/*****************************************************************
Decode the command line
*****************************************************************/

	if (np != 5) {
		print_help(p[0]);
		return 1;
	}

	log = fopen("setup.txt","w");
	if(log == NULL) {
		printf("Unable to open \"setup.txt\" error code %i (%s)\n", errno, strerror(errno));
		return 1;
	}

	fprintf (log,"Disk: %s\n",p[1]);
	fprintf (log,"Host: %s\n",p[2]);
	fprintf (log,"Operator: %s\n",p[3]);
	fprintf (log,"OS: %s\n",p[4]);
	fprintf (log,"Date: %s\n",ctime(&from));

	return 0;
}
