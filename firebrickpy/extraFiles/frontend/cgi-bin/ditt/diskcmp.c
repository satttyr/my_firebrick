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
static char *SCCS_ID[] = {"@(#) diskcmp.c Linux Version 1.2 Created 02/18/05 at 08:49:40",
	__DATE__,__TIME__};

# include <stdio.h>
# include "zbios.h"
# include <string.h>
# include <time.h>
# include <malloc.h>

/*****************************************************************
Compare two disks

diskcmp test-case host operator source-disk source-fill-byte dst-disk dst-fill-byte

The assumption is that the source-disk has been copied to the dst-disk

High level design:
	decode command line
	open source and dst disks
	for each sector (address) common to both disks
		read src sector
		read dst sector
		compare: increment count of equal_sectors or differing_sectors
	log results
	if src has more sectors than destination then exit
	else if src and dst are same size then exit
	else for each sector remaining on the dst
		read dst sector
		examine: count zero filled, src filled, dst filled, other filled and
			not filled sectors
	log results
*****************************************************************/

void print_help(char *p) {
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator src-drive src-fill dst-drive dst-fill [-options]\n",p);
	printf ("-comment \" ... \"\tDescriptive comment\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is cmplog.txt)\n");
	printf ("-h\tPrint this option list\n");
}

main (int np, char **p) {
	char		src_drive[NAME_LENGTH],
			dst_drive[NAME_LENGTH]; /* drive devices */
	int		help = 0,
			status,
			i;
	static disk_control_ptr src_disk,
			dst_disk; /* disk information */
	off_t		lba = 0, /* index for looping through disk sectors */
			common, /* number of sectors common to source and dst */
			diffs = 0, /* number of sectors that do not match */
			nz, /* number of zero bytes in current sector */
			nfill,/* number of filled bytes in current sector */
			src_ns,dst_ns, /* number of sectors on src and dst */
		/* counts: sectors that ... */
			byte_diffs = 0,
			match = 0,
			zero = 0,
			sfill = 0,
			dfill = 0,
			ofill = 0,
			other = 0;
	int		big_src = 0,
			big_dst = 0,
			is_diff,
			src_status,
			dst_status; /* read status codes (should be zero) */
	static unsigned char *src_buff,
			*dst_buff; /* current src and dst sector data */
	static time_t	from; /* program start time */
	FILE		*log;  /* the log file */
	int		is_debug = 0;
	unsigned char	other_fill_char,
			src_fill_char,
			dst_fill_char; /* the fill characters */
	int		fill_char,
			other_fill_seen = 0;
	off_t		n_src_err = 0,
			n_dst_err = 0; /* count of number of read errs */
	char		comment[NAME_LENGTH] = "",
			access[2] = "a"; /* the user comment */
	char		log_name[NAME_LENGTH] = "cmplog.txt";
								/* sectors that ... */
	range_ptr d_r = create_range_list(), /* ... differ (do not match) */
			zf_r = create_range_list(), /* ... zeros filled */
			sf_r = create_range_list(),/* ... source filled */
			df_r = create_range_list(), /* ... dst filled */
			of_r = create_range_list(), /* ... filled with something else */
			o_r = create_range_list(); /* ... are not filled */

	time(&from);
	src_disk = dst_disk = NULL;

/*****************************************************************
Get the command line
*****************************************************************/
	if (np < 8) {
		print_help(p[0]); /* not enough parameters */
		return 1;
	}

	strncpy(src_drive, p[4], NAME_LENGTH - 1);
	strncpy(dst_drive, p[6], NAME_LENGTH - 1);

	printf ("Src drive %s dst drive %s\n",src_drive,dst_drive);

	sscanf (p[5],"%2x",&fill_char);
	src_fill_char = fill_char;
	sscanf (p[7],"%2x",&fill_char);
	dst_fill_char = fill_char;

	printf ("Src fill 0x%02X dst fill 0x%02X\n",src_fill_char,dst_fill_char);

	for (i = 8; i < np; i++) { /* optional parameters */
		if (strcmp (p[i],"-h") == 0) help = 1;
		else if (strcmp (p[i],"-debug")== 0) is_debug = 1;
		else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i], "-log_name") == 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else strncpy(log_name, p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-comment")== 0) {
			if (++i >= np) {
				printf ("%s: comment required with -comment\n",	p[0]);
				help = 1;
			} else strncpy (comment, p[i], NAME_LENGTH - 1);
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
	if (help) {
		print_help(p[0]);
		return 0;
	}
/*****************************************************************
Start log file
*****************************************************************/
	log = log_open(log_name,access,comment,SCCS_ID,np,p);
	src_disk = open_disk (src_drive,&status);
	if (status) {
		printf ("%s could not access src drive %s status code %d\n",
			p[0],src_drive,status);
		fprintf (log,"%s could not access src drive %s status code %d\n",
			p[0],src_drive,status);
		return 1;
	}
	log_disk(log,"Source",src_disk);
	dst_disk = open_disk (dst_drive,&status);
	if (status){
		printf ("%s could not access dst drive %s status code %d\n",
			p[0],dst_drive,status);
		fprintf (log,"%s could not access dst drive %s status code %d\n",
			p[0],dst_drive,status);
		return 1;
	}
	log_disk(log,"Destination",dst_disk);
	src_ns = n_sectors(src_disk);
	dst_ns = n_sectors(dst_disk);

	if (src_ns != dst_ns){
		if ( src_ns < dst_ns){
			common = src_ns;
			big_dst = 1;
		}
		else {
			common = dst_ns;
			big_src = 1;
		}
	}
	else common = src_ns; 
/*****************************************************************
Main scan loop: read corresponding sectors and compare
*****************************************************************/
	for (lba = 0; lba < common; is_debug?(lba+=100):lba++){
		is_diff = 0;
		feedback (from,0,lba,big_dst?dst_ns:common);
		src_status = read_lba(src_disk,lba,&src_buff);
		dst_status = read_lba(dst_disk,lba,&dst_buff);
		if (src_status) { /* if bad sectors, keep list of first 10 */
			n_src_err++;
			if (n_src_err < 11) {
				fprintf (log,"src read error 0x%02X on track starting at lba %llu\n",
					src_status,lba);
				printf ("src read error 0x%02X at lba %llu\n",src_status,lba);
			} else if (n_src_err == 11) {
				fprintf (log,"... more src read errors\n");
				printf ("... more src read errors\n");
			}
			continue;
		}
		if (dst_status) { /* if bad sectors, keep list of first 10 */
			n_dst_err++;
			if (n_dst_err < 11) {
				fprintf (log,"dst read error 0x%02X on track starting at lba %llu\n",
					dst_status,lba);
				printf ("dst read error 0x%02X at lba %llu\n",dst_status,lba);
			} else if (n_dst_err == 11) {
				fprintf (log,"... more dst read errors\n");
				printf ("... more dst read errors\n");
			}
			continue;
		}
		for (i = 0; i < BYTES_PER_SECTOR; i++){  /* count bytes different */
			if (src_buff[i] != dst_buff[i]){
				is_diff = 1;
				byte_diffs++;
			}
		}
		if (is_diff){/* sectors do not match */
			diffs++;
			add_to_range (d_r,lba);
		}
		else {
			match++;
		}
	}
	/* log results for corresponding sectors */
	fprintf (log,"Sectors compared: %8llu\n",common);
	fprintf (log,"Sectors match:    %8llu\n",match);
	fprintf (log,"Sectors differ:   %8llu\n",diffs);
	if (n_src_err + n_dst_err){ /* note any I/O errors */
		fprintf (log, "Sectors skipped:  %8llu (due to %llu src & %llu dst I/O errors)\n",
			n_src_err+n_dst_err,n_src_err,n_dst_err);
	}
	fprintf (log,"Bytes differ:     %8llu\n",byte_diffs);
	print_range_list(log,"Diffs range",d_r);
	if (big_src){
		fprintf (log,"Source (%llu) has %llu more sectors than destination (%llu)\n",
			src_ns,src_ns - dst_ns,
			dst_ns);
	}
	else if (big_dst){ /* examine remainder of a larger destination */
		fprintf (log,"Source (%llu) has %llu fewer sectors than destination (%llu)\n",
			src_ns,dst_ns - src_ns,
			dst_ns);
		zero = 0;
		ofill = sfill = dfill = 0;
		other = 0;
		printf ("Destination larger than source; scanning %llu sectors\n",
			dst_ns-common);
		for (lba = common; lba < dst_ns; is_debug?(lba+=100):lba++){
			feedback (from,0,lba,dst_ns);
			dst_status = read_lba(dst_disk,lba,&dst_buff);
			if (dst_status){
				n_dst_err++;
				if (n_dst_err < 11){
					fprintf (log,"dst read error 0x%02X on track starting at lba %llu\n",
						dst_status,lba);
					printf ("dst read error 0x%02X at lba %llu\n",dst_status,lba);
				}
				if (n_dst_err == 11) {
					fprintf (log,"... more dst read errors\n");
					printf ("... more dst read errors\n");
				}
				continue;
			}
			nz = 0;
			nfill = 0;
			for (i = 0; i < BYTES_PER_SECTOR; i++) {
				if (dst_buff[i] == 0) nz++;
				else if (dst_buff[i] == dst_buff[BUFF_OFF]) nfill++;
			}
			if (nz == BYTES_PER_SECTOR) {zero++; add_to_range(zf_r,lba);}
			else if (nfill > 480){  /* filled sector: figure out src, dst or other */
					if (dst_buff[BUFF_OFF] == src_fill_char){
						sfill++;
						add_to_range(sf_r,lba);
					}
					else if (dst_buff[BUFF_OFF] == dst_fill_char){
						dfill++;
						add_to_range(df_r,lba);
					}
					else {
						ofill++;
						add_to_range(of_r,lba);
						other_fill_seen = 1;
						other_fill_char = dst_buff[BUFF_OFF];
					}
			} else {
				other++;
				add_to_range (o_r,lba);
			}
		}
		/* log results of scan of extra dst sectors */
		fprintf (log,"Zero fill:          %8llu\n",zero);
		fprintf (log,"Src Byte fill (%02X): %8llu\n",src_fill_char,sfill);
		if (src_fill_char == dst_fill_char)
			fprintf (log,"Dst Fill Byte same as Src Fill Byte\n");
		else fprintf (log,"Dst Byte fill (%02X): %8llu\n",dst_fill_char,dfill);
		if (other_fill_seen)
				fprintf (log,"Other fill (%02X):    %8llu\n",other_fill_char,ofill);
		else fprintf (log,"Other fill:         %8llu\n",ofill);
		fprintf (log,"Other no fill:      %8llu\n",other);
		print_range_list (log,"Zero fill range: ",zf_r);
		print_range_list (log,"Src fill range: ",sf_r);
		print_range_list (log,"Dst fill range: ",df_r);
		print_range_list (log,"Other fill range: ",of_r);
		print_range_list (log,"Other not filled range: ",o_r);
	}
	fprintf (log,"%llu source read errors, %llu destination read errors\n",
		n_src_err,n_dst_err);

	log_close(log,from);
	return 0;
}
