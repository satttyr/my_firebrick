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
static char *SCCS_ID[] = {"@(#) corrupt.c Linux Version 1.2 Created 02/18/05 at 08:49:40",
			__DATE__,__TIME__};
/***** Author: Dr. James R. Lyle, NIST/SDCT/SQG ****/
/* Modified by Kelsey Rider, NIST/SDCT June 2004 */
# include <features.h>
# include <unistd.h>
# include <stdio.h>
# include "zbios.h"
# include <time.h>
# include <string.h>
# include <malloc.h>
# include <fcntl.h>
# include <errno.h>
/*****************************************************************
Corrupt an image file
CORRUPT is used to corrupt an image file by changing a
single selected byte in an image file.
CORRUPT logs the original content of the selected byte,
the replacement value, the offset of the byte within the file and
the file name.

program outline
	get command line
	open the file
	read byte to change
	write back new value
	read back to confirm
	log results

*****************************************************************/

void print_help(char *p) {
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator file_name offset hex_value [-options]\n",p);
	printf ("-comment \" ... \"\tGive comment on command line\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is corlog.txt)\n");
	printf ("-h\tPrint this option list\n");
}

# define ERR -1
main (int np, char **p) {
	int		help = 0,
			i, /* loop index */
			f; /* file descriptor */
	int		status; /* return code for read/write operations */
	static time_t	from; /* start time */
	FILE		*log; /* the log file */
	static char	comment[NAME_LENGTH] = "",
			log_name[NAME_LENGTH] = "corlog.txt",
			access[2] = "a";
	off_t		at, /* file offset (location) to make change */
			seek_return; /* return value from seek call */
	unsigned char	old_char = 'a', /* the char originally in the file */
			new_char = 'Z', /* the replacement character */
			verify_char = '0'; /* after writting, read the char back to make sure */

	time(&from);
	printf ("\n%s compiled at %s on %s\n", p[0],
		__TIME__,__DATE__);

		/* get the command line */
	if (np < 7) help = 1;
	for (i = 7; i < np; i++) {
		if (strcmp (p[i],"-h") == 0) {help = 1; break;}
		else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i], "-log_name") == 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else strncpy(log_name, p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-comment")== 0) {
			i++;
			if (i >= np){
				printf ("%s: -comment option requires a comment\n",p[0]);
				help = 1;
			} else strncpy (comment,p[i], NAME_LENGTH - 1);
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
	if (help){
		print_help(p[0]);
		return 0;
	}
	/* open the file named in p[4] */
	f = open (p[4],O_RDWR);
	if (f == ERR){
		printf ("%s: Open %s failed with error %i (%s)\n",p[0],p[4],errno,strerror(errno));
		return 1;
	}
	/* get the offset value */
	status = sscanf (p[5],"%llu",&at);
	if (status != 1){
		printf ("%s: Offset value (%s) is not valid\n",p[0],p[5]);
		return 1;
	}
	/* get the replacement character */
	status = sscanf (p[6],"%02X",&new_char);
	if (status != 1){
		printf ("%s: Replacement value (%s) is not valid\n",p[0],p[6]);
		return 1;
	}
	/* position to read from the offset */
	seek_return = lseek (f,at,SEEK_SET);
	if (seek_return == ERR){
		printf ("%s: Offset %llu is not valid for %s\n",p[0],at,p[4]);
		printf("Error: %i (%s)\n", errno, strerror(errno));
		return 1;
	}
	/* read the current contents */
	status = read (f,&old_char,1);
	if (status != 1){
		printf ("%s: Read failed\n",p[0]);
		return 1;
	}
	/* reset the file position */
	seek_return = lseek (f,at,SEEK_SET);
	if (seek_return == ERR){
		printf ("%s: Seek to %llu on %s after read failed\n\n",p[0],at,p[4]);
		return 1;
	}
	/* write the new (replacement) value */
	status = write (f,&new_char,1);
	if (status != 1){
		printf ("%s: Write failed\n",p[0]);
		return 1;
	}
	/* Commit data to file */
	status = mysync(f);
	/* reset the file position */
	seek_return = lseek (f,at,SEEK_SET);
	if (seek_return == ERR){
		printf ("%s: Seek to %llu on %s after write failed\n",p[0],at,p[4]);
		return 1;
	}
	/* read back what was written */
	status = read (f,&verify_char,1);
	if (status != 1){
		printf ("%s: Verify read failed\n",p[0]);
		return 1;
	}
	/* Verify match */
	if (verify_char != new_char){
		printf ("%s: verify failed: char read back (0x%02X) should be (0x%02x)\n",
			p[0],old_char,new_char);
		return 1;
	}
	 /* ran OK, so log results */
	log = log_open(log_name,access,comment,SCCS_ID,np,p);

	fprintf (log,"Change byte %llu of file %s from 0x%02X to 0x%02X\n",
		at,p[4],old_char,new_char);
	printf ("Change byte %llu of file %s from 0x%02X to 0x%02X\n",
		at,p[4],old_char,new_char);
	log_close (log,from);
	return 0;
}
