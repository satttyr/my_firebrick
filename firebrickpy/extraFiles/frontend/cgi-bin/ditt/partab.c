static char *SCCS_ID[] = {"@(#) partab.c Linux Version 1.4 Created 03/21/05 at 09:09:30",
	__DATE__,__TIME__};
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
/***** Author: Dr. James R. Lyle, NIST/SDCT/SQG ****/
/***** Revised by Ben Livelsberger, NIST/SDCT   ****/
/* Modified by Kelsey Rider, NIST/SDCT June 2004 */
# include <stdio.h>
# include <string.h>
# include "zbios.h"   
# include <malloc.h>
# include <time.h>

/******************************************************************************
Print a partition table
	open disk
	open logfile
	log disk
	print partition table
	close log
******************************************************************************/


/*	Helper function
	return index of filename, without leading path information
*/
char * filename(char* path) {
	int i;

	if(strlen(path) < 1) return path;

	for(i = strlen(path) - 1; i >= 0; i--) {
		if(path[i] == '/') return &path[i+1];
	}

	return path;
}

void print_help(char *p /* program name */)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator drive label [-options]\n",p);
	printf ("-all\tList extended partitions\n");
	printf ("-comment \" ... \"\tComment for log file\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse a different log file (default is pt-label-log.txt\n\tand is written to the current directory)\n");
	printf ("-h\tPrint this option list\n");
}

int main (int np, char **p)
{
	char		drive[NAME_LENGTH] = "/dev/hda";
	int		lname_given = 0,
			status,
			i,
			all,
			help = 0;
	static disk_control_block *dd;
	pte_rec		pt[4];
	FILE		*log;
	char		comment [NAME_LENGTH] = "",
			log_name[NAME_LENGTH],
			access[2] = "a";
	time_t		from;

	/*_stklen = 2*_stklen;*/
	time(&from);
	printf ("\n%s compiled at %s on %s\n", p[0],
		__TIME__,__DATE__);

/* get command line */
	if (np < 6) help = 1;
	else strncpy(drive, p[4], NAME_LENGTH - 1);
	for (i = 6; i < np; i++){
		if (strcmp(p[i],"-all") == 0) all = 1;
		else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i],"-log_name")== 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else {
				sprintf (log_name,"%s",p[i]);
				lname_given = 1;
			}
		} else if (strcmp(p[i],"-comment") == 0) {
			i++;
			if (i < np) strncpy (comment, p[i], NAME_LENGTH - 1);
			else help = 1;
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
	if (help) {
		print_help(p[0]);
		return 0;
	}
	status = 1;
	printf ("Drive %s\n",drive);
/* open log file */
	if (lname_given == 0) sprintf (log_name,"pt-%s-log.txt", filename(&p[4][5]));

	log = log_open (log_name,access,comment,SCCS_ID,np,p);

/* open and log disk */
	dd = open_disk (drive,&status);
	if (status) {
		printf ("%s could not access drive %s, status code %d\n",p[0],
			drive,status); 
		fprintf (log,"%s could not access drive %s, status code %d\n",p[0],
			drive,status);
		return 1;
	}
	fprintf (log,"Drive label: %s\n",p[5]);
	log_disk (log,"Partition table",dd);

/* get and print partition table */
	status = get_partition_table(dd,pt);
	if (status == 0) print_partition_table(stdout, pt, 1, all);
	else if (status == -1) printf ("No partition table signature\n");
	else printf ("Error reading partition table, code %d\n",status);

	if (status == 0) print_partition_table(log, pt, 1, all);
	if (status)fprintf (log,"Error reading partition table, code %d\n",status);
	log_close (log,from);
	return 0;
}
