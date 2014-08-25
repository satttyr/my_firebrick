static char *SCCS_ID[] = {"@(#) seccmp.c Linux Version 1.3 Created 03/18/05 at 14:39:56",
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
/* Modified by Kelsey Rider, NIST/SDCT June 2004 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include "zbios.h"
# include <malloc.h>
# include <time.h>
/*****************************************************************
Compare two disk sectors
The assumption is that the source has been copied to the dst
It is also assumed that the diskwipe program was run with
source-fill-byte for the source disk
and dst-fill-byte for the destination disk

Report (in log file):
number of bytes different between src sector and dst sector
If a sector is filled by diskwipe, print the address string
that starts in the first byte of the sector
*****************************************************************/

int cmp_sec (	disk_control_ptr src_dcb,	/* source disk */
		disk_control_ptr dst_dcb,	/* destination disk */
		off_t		src_base,	/* base LBA of source address */
		off_t		src_offset,	/* offset to add to base */
		off_t		dst_base,	/* ditto for dst */
		off_t		dst_offset,
		FILE		*log		/* the log file */
) {
	int		src_status,	/* status of source sector read */
			dst_status,	/* status of dst sector read */
			i, j, k,	/* loop indices */
			need_separator = 0, /* need to print a separator between byte blocks */
			print,		/* indicates that a byte pair in the current block differs */
			n_diff,		/* count of number of bytes not matching */
			src_fill, dst_fill, zero_fill; /* flags indicating type of fill */
	off_t		src_lba,dst_lba;/* sector addresses */
	unsigned char	*sb, *db,	/* sector buffers */
			src_fill_char,	/* the src fill character, if one exists */
			dst_fill_char,	/* the dst fill character, if one exists */
			buff[26];	/* for outputting beginning of sector */


/*****************************************************************
get source and destination sectors and check read status
*****************************************************************/
	src_lba = src_base + src_offset;
	dst_lba = dst_base + dst_offset;
	src_status = read_lba (src_dcb, src_lba, &sb);
	dst_status = read_lba (dst_dcb, dst_lba, &db);
	if (src_status) {
		fprintf (log,"Src Read error 0x%02X at LBA %llu\n",
			src_status, src_lba); 
		printf ("Src Read error 0x%02X at LBA %llu\n",
			src_status, src_lba);
	} 
	if (dst_status) {
		printf ("Read error on destination\n");
		fprintf (log,"Dst Read error 0x%02X at LBA %llu\n",
			dst_status, dst_lba); 
		printf ("Dst Read error 0x%02X at LBA %llu\n",
			dst_status, dst_lba);
	}

	if(src_status || dst_status) return src_status;
/*****************************************************************
Compare the source sector to the destination
*****************************************************************/
	printf ("Src %llu Dst %llu\n",src_lba,dst_lba);
	fprintf (log,"\nCompare sectors at: Src %llu (%llu+%llu) Dst %llu (%llu+%llu)\n",
		src_lba, src_base, src_offset, dst_lba, dst_base, dst_offset);
	src_fill_char = sb[BUFF_OFF]; /* if there is a fill then this would be it */
	dst_fill_char = db[BUFF_OFF];
	src_fill = 1; /* assume filled until shown otherwise */
	dst_fill = 1;
	for (i = BUFF_OFF + 1; i < BYTES_PER_SECTOR; i++)
		if (sb[i] != src_fill_char)
			{src_fill = 0; break;} /* src not src filled */
/*****************************************************************
Check for sector completely filled with the same byte
*****************************************************************/
	if (src_fill){
		n_diff = 0;
		for (i = 0; i < BUFF_OFF; i++)
			if (sb[i] != sb[BUFF_OFF]) n_diff ++;
		if (n_diff < 3) src_fill = 0; /* should be at least 3 diffs */
		else n_diff = 0;
	}
/*****************************************************************
*****************************************************************/
	for (i = BUFF_OFF; i < BYTES_PER_SECTOR; i++)
		if (db[i] != dst_fill_char)
			{dst_fill = 0; break;} /* dst not dst filled */
	zero_fill = dst_fill && (dst_fill_char == '\0'); /* filled ... but with zero */
	if (zero_fill)
		for (i = 0; i < BUFF_OFF; i++) /* is it really zero filled? */
			if (db[i] != '\0') {zero_fill = 0; dst_fill = 0; break;}
/*****************************************************************
Check for sector completely filled with the same byte
*****************************************************************/
	if (dst_fill){
		n_diff = 0;
		for (i = 0; i < BUFF_OFF; i++)
			if (db[i] != db[BUFF_OFF]) n_diff ++;
		if (n_diff < 3) dst_fill = 0; /* should be at least 3 diffs */
	}
	n_diff = 0;
/*****************************************************************
Both source and dst are filled. Sectors may be same or different.
Only neet to report that sectors are filled.
*****************************************************************/
	if (src_fill && dst_fill){
		for (i = 0; i < BYTES_PER_SECTOR; i++)
			if (db[i] != sb[i]) n_diff++;

		/* copy beginning of sector (diskwipe-set stuff) into buffer */
		strncpy(buff, sb, 25);
		buff[25] = '\0';
		printf ("Src filled by %02X from %s\n", src_fill_char, buff);
		fprintf (log,"Src filled by %02X from %s\n", src_fill_char, buff);
		if (zero_fill){
			printf ("Dst zero filled\n"); 
			fprintf (log,"Dst zero filled\n");
		}
		else {
			/* copy beginning of sector (diskwipe-set stuff) into buffer */
			strncpy(buff, db, 25);
			buff[25] = '\0';
			printf ("Dst filled by %02X from %s\n", dst_fill_char, buff); 
			fprintf (log,"Dst filled by %02X from %s\n", dst_fill_char, buff);
		}
	}
/*****************************************************************
Some byte pairs do not match, print the sectors in blocks of
16 bytes at a time. DON'T PRINT A BLOCK IF ALL BYTE PAIRS IN THE
BLOCK MATCH.
*****************************************************************/
	else for (i = 0; i < BYTES_PER_SECTOR; i+= 16){
		print = 0;
		for (j = 0; j < 16; j++)
			if (sb[i+j] != db[i+j]) {n_diff++; print = 1;} /* need to print */
		if (print) {
			if (need_separator) {
				for (k = 0; k < (8+3*16); k++)fprintf(log,"-");
					fprintf (log,"\n");
			} else need_separator = 1;
			printf ("src %3d:",i); /* print byte offset of first byte in block */
			fprintf (log,"Src %3d:",i);
			for (j = 0; j < 16; j++){ /* print 16 source bytes in hex */
				printf (" %02X",sb[i+j]);
				fprintf (log," %02X",sb[i+j]);
			}
			printf ("\ndiff   :");
			fprintf (log,"\ndiff   :");
			for (j = 0; j < 16; j++){ /* mark byte pairs that differ with asterisks */
				printf (" %2s", (sb[i+j] != db[i+j])?"**":"");
				fprintf (log," %2s", (sb[i+j] != db[i+j])?"**":"");
			}
			printf ("\n");
			printf ("Dst %3d:",i); 
			fprintf (log,"\n");
			fprintf (log,"Dst %3d:",i);
			for (j = 0; j < 16; j++){  /* print 16 dst bytes in hex */
				printf (" %02X",db[i+j]); 
				fprintf (log," %02X",db[i+j]);
			}
			printf ("\n"); 
			fprintf (log,"\n");
		}
	}
/*****************************************************************
 log summary results
*****************************************************************/
	printf ("%d bytes different\n",n_diff); 
	fprintf (log,"%d bytes different\n\n",n_diff);
	return 0;
}

/*****************************************************************
Print usage information (command line format & options)
*****************************************************************/
void print_help(char *p)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator src-drv src-label dst-drv dst-label [-options]\n",p);
	printf ("-comment \"...\"\tDescriptive comment\n");
	printf ("-sector src_lba dst_lba\tSpecify the sectors to compare\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is seclog.txt)\n");
	printf ("-h\tPrint this option list\n");
}

main (int np, char **p)
{
	int		help = 0,
			status = 0,
			i;
	static disk_control_ptr src_dcb; /* source disk */
	char		src_drive[NAME_LENGTH] = "/dev/hda";

	static disk_control_ptr dst_dcb; /* destination disk */
	char		dst_drive[NAME_LENGTH] = "/dev/hdb";
	off_t		src_base,
			src_offset = 0,
			dst_base,
			dst_offset = 0;
	int		interactive = 1; /* assume user wants interactive mode unless
						overriden on command line */
	FILE		*log;
	char		comment [NAME_LENGTH] = "",
			log_name [NAME_LENGTH] = "seclog.txt",
			access[2] = "a";
	static time_t	from;

	/*_stklen = 2*_stklen;*/
	printf ("\n%s compiled at %s on %s\n", p[0],
		__TIME__,__DATE__);
	if (np < 8) help = 1;
	else {
/*****************************************************************
get source and destination drives (in hex) from command line
*****************************************************************/

		strncpy(src_drive, p[4], NAME_LENGTH - 1);
		strncpy(dst_drive, p[6], NAME_LENGTH - 1);

		if (!strcmp(src_drive, dst_drive)) {
			help = 1;
			printf ("Source and destination drives must be different\n");
		}
	}

/*****************************************************************
get command line
*****************************************************************/
	for (i = 8; i < np; i++) {
		if (strcmp (p[i],"-h") == 0) help = 1;
		else if (strcmp (p[i],"-log_name")== 0) {
			i++;
			if (i >= np) {
				printf ("%s: -log_name option requires a logfile name\n",p[0]);
				help = 1;
			} else strncpy (log_name,p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-comment")== 0) {
			i++;
			if (i >= np){
				printf ("%s: -comment option requires a comment\n",p[0]);
				help = 1;
			} else strncpy (comment,p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i],"-sector")== 0) {
			i+=2;
			if (i >= np){
				printf ("%s: -sector option requires src and dst sector LBA addresses\n",p[0]);
				help = 1;
			} else {
				sscanf (p[i-1],"%llu",&src_base);
				sscanf (p[i],"%llu",&dst_base);
				interactive = 0;
			}
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
/*****************************************************************
If there is a problem on command line, then print help message
*****************************************************************/
	if (help) {
		print_help(p[0]);
		return 0;
	}
	time(&from);
/*****************************************************************
Open log file; open source and destination
*****************************************************************/
	log = log_open (log_name,access,comment,SCCS_ID,np,p);

	src_dcb = open_disk (src_drive,&status);
	printf ("Open disk %s status code %d\n", src_drive, status);
	if(status) return status;

	dst_dcb = open_disk (dst_drive,&status);
	printf ("Open disk %s status code %d\n", dst_drive, status);
	if(status) return status;

	log_disk (log,"Source disk",src_dcb); 
	log_disk (log,"Destination disk",dst_dcb);
/*****************************************************************
If source and destination given on command line, compare the
sectors specified. Otherwise go into interactive mode and prompt
for disk addresses (LBA). Since some compare programs sometimes
provide addresses relative to a partition, prompt for addresses
in the form of base (partition start) + offset (relative to partition
start). An absolute LBA would be entered as the base with an offset
of zero (or vice versa).
*****************************************************************/
	if (interactive) {
		printf ("Enter src (base offset) dst (base offset) (CTRL-D to quit): ");
		while (EOF != scanf ("%llu%llu%llu%llu",&src_base,&src_offset,
			&dst_base,&dst_offset)){
			status = cmp_sec (src_dcb,dst_dcb,src_base,src_offset,
				dst_base,dst_offset,log);
			printf ("Enter src (base offset) dst (base offset) (CTRL-D to quit): ");
		}
	} else { /* one address pair from the command line */
		status = cmp_sec (src_dcb,dst_dcb,src_base,src_offset,
			dst_base,dst_offset,log);
	}

	log_close(log,from);
	return 0;
}
