static char *SCCS_ID[] = {"@(#) diskwipe.c Linux Version 1.4 Created 03/18/05 at 14:49:21",
				__DATE__,__TIME__};
static char *test_version = "*** TEST VERSION ";
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
# include <time.h>
# include <malloc.h>
# include <unistd.h>

/*****************************************************************
Write a known pattern to each sector of a disk:
	bytes 0-13	C/H/S address of the sector
	byte 14     blank character
	bytes 15-24 LBA address of the sector
	byte 25		NULL (0)
	bytes 26-511 Fill Byte
*****************************************************************/


/*****************************************************************
Wipe n_sect sectors of the disk with fill
	d describes the disk to wipe
	n_sect is the number of sectors to wipe
	fill is the fill byte
	start_time is used to estimate time remaining
	heads is used to specify an alternate disk geometry for
		the C/H/S address written to the disk (see note in main)
*****************************************************************/
int do_wipe(disk_control_ptr d, off_t n_sect,unsigned char fill,
	time_t start_time, int heads)
{
	static off_t	track,
			cylinder,
			head,
			sector,
			s,
			hpc, /* heads per cylinder */
			spt = DISK_MAX_SECTORS; /* sectors per track */
	unsigned char	*b = (unsigned char *) &d->buffer[0][0];
	chs_addr	at;
	int		i;
	off_t		from = 0,
			up_to = n_sect;

	hpc = (off_t) n_heads(d);
	if(!hpc) return 1; /* to prevent divide-by-zero error */

	memset (b,fill,63*512);
	printf ("Wipeout from %llu up to %llu\n",from,up_to);
	if (heads)printf ("Override heads: %d\n",heads);

	for (s = from; s < up_to; s++){
		track = s/spt;
		sector = s%spt + 1;
		cylinder = track/hpc;
		head = track%hpc;
		at.cylinder = cylinder;
		at.head = head;
		at.sector = sector;
		if (heads){
			cylinder = track/heads;
			head = track%heads;
		}
		sprintf ((char *)(&(d->buffer[sector-1][0])),
			"%05llu/%03llu/%02llu %012llu",cylinder,head,sector,s);
		if ((sector == DISK_MAX_SECTORS) || ((s+1) == up_to)) {
			if (((s+1) == up_to) && (sector != DISK_MAX_SECTORS))
				printf ("Note: Partial last track (%llu) written at sector %llu\n", sector,s);
			at.sector = 1;
			disk_write (d,&at);
		}
		feedback (start_time, from, s, up_to);
	}

	/* Sync file (make sure all writing is committed) */
	return mysync(d->fd);
}

/*****************************************************************
Print the command line format & options
	p is the command name
*****************************************************************/
void print_help(char *p)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator drive fill [-options]\n",p);
	printf ("-src\tWipe a source disk\n");
	printf ("-media\tWipe a media disk\n");
	printf ("-dst\tWipe a destination disk (default)\n");
	printf ("-heads nnn\tOveride number of heads from BIOS with nnn\n");
	printf ("-comment \" ... \"\tGive a comment on command line\n");
	printf ("-noask\tSupress confirmation dialog\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse a different log file (default is wipedlog.txt)\n");
	printf ("-h\tPrint this option list\n");
}


main (int np, char **p)
{
	char		drive[NAME_LENGTH] = "/dev/hda"; /* default to primary IDE master */
	int		help = 0,
			ifill,
			n_logs = 0;
	int		status,
			i,
			is_debug = 0,
			ask = 1,
			hd = 0;
	off_t	 	ns;
	static disk_control_block *dd;
	unsigned char	fill;
	char		ans[NAME_LENGTH];
	static time_t	from;
	FILE		*log;
	char		comment[NAME_LENGTH] = "",
			log_name[NAME_LENGTH] = "wipedlog.txt",
			access[2] = "a";

	time(&from);
	printf ("%s %s%s\n",p[0],ctime(&from),SCCS_ID[0]);
	printf ("Compiled %s %s with CC Version %s\n",__DATE__,
		__TIME__,__VERSION__);
	printf("cmd: ");
	for(i=0; i<np; i++) {
		printf(" %s", p[i]);
	}
	printf("\n");


	if (np < 6) help = 1;
	else strncpy(drive, p[4], NAME_LENGTH - 1);
	printf ("Drive %s\n",drive);
	for (i = 6; i < np; i++) {
		if (strcmp (p[i],"-h") == 0) help = 1;
		else if (strcmp (p[i],"-src")== 0){strncpy(log_name, "wipeslog.txt", NAME_LENGTH - 1); n_logs++;}
		else if (strcmp (p[i],"-media")== 0){strncpy(log_name, "wipemlog.txt", NAME_LENGTH - 1); n_logs++;}
		else if (strcmp (p[i],"-dst")== 0){strncpy(log_name, "wipedlog.txt", NAME_LENGTH - 1); n_logs++;}
		else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i],"-noask") == 0) ask = 0;
		else if (strcmp (p[i],"-comment")== 0){
			i++;
			if (i >= np){
				printf ("%s: -comment option requires a comment\n",p[0]);
				help = 1;
			} else strncpy (comment,p[i], NAME_LENGTH - 1);
		}  
		else if (strcmp (p[i],"-log_name")== 0){
			i++;
			if (i >= np){
				printf ("%s: -log_name option requires a logfile name\n",p[0]);
				help = 1;
			} else {strncpy(log_name, p[i], NAME_LENGTH - 1);n_logs++;}
		}
		else if (strcmp (p[i],"-heads")== 0){
			i++;
			if (i >= np){
				printf ("%s: -heads option requires a value\n",p[0]);
				help = 1;
			} else {sscanf (p[i],"%d",&hd);printf ("Set heads to %d\n",hd);}
/* **** NOTE about heads value
We have seen situations where the heads value obtained for the
BIOS by the interrupt 13 command 48 is off by one, i.e., the disk
has 64 heads but the value returned by the 48 command is 63. This
does not cause any problems in disk addressing since the C/H/S values
are not used, (LBA numbers are used). However, the C/H/S values written
by this program get out of sync with the real C/H/S addresses.
For example, 0/62/63 is followed by 1/0/1 (instead of 0/63/1).
This /heads option can be used the keep the values in sync with
the actual addresses.
*/
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
	if (help) {
		print_help(p[0]);
		return 0;
	}
	if (n_logs > 1) {
		printf ("Note: multiple log file names specified\nEnter \"y\" if %s is ok:", log_name);
		scanf("%s", ans);
		if(ans[0] != 'y') return 1;
	}
	if (SCCS_ID[0][0] == '%') SCCS_ID[0] = test_version;
	log = log_open(log_name,access,comment,SCCS_ID,np,p);

	time(&from);
	sscanf (p[5],"%2x",&ifill); /* note first two characters of label are HEX */
	fill = ifill;
	if (ask){
		printf ("This program will erase (WIPEOUT) disk %s OK? (y/n)?",
			drive);
		scanf ("%s",ans);
		if (ans[0] != 'y') return 1;
		printf ("Do you want to WIPEOUT disk %s with %02X? (y/n)?",
			drive,ifill);
		scanf ("%s",ans);
		if (ans[0] != 'y') return 1;
	}

	dd = open_disk (drive,&status);
	if (status){
		printf ("%s could not access drive %s status code %d\n",
			p[0],drive,status);
		fprintf (log,"%s could not access drive %s status code %d\n",
			p[0],drive,status);
		return 1;
	}
	log_disk(log,"Wipe",dd);
	if (hd) {
		if(hd < 0) {
			printf("Invalid value (%d) for number of heads\n", hd);
			return 1;
		}
		fprintf (log,"Override number of heads from %llu to %d\n",
			dd->disk_max.head,hd);
	}
/*	print_dcb(dd);       */
	ns = n_sectors(dd);
	if (is_debug) ns = 100;
	status = do_wipe(dd,ns,fill,from,hd);
	if (status) {
		printf ("error code %d in %s\n",status,p[0]);
		fprintf (log,"error code %d in %s\n",status,p[0]);
	} else {
		fprintf (log,"%llu sectors wiped with %2X\n",ns,ifill);
		printf ("%llu sectors wiped with %2X\n",ns,ifill);
	}
	log_close (log,from);
	return status;
}
