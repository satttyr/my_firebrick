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

/* NOTE:  This file uses LBA to refer to a sector number, not an individual byte! */
# include <stdio.h>
# include "zbios.h"
# include <string.h> 
# include <malloc.h>
# include <time.h>
static char *SCCS_ID[] = {"@(#) partcmp.c Linux Version 1.3 Created 03/15/05 at 17:25:33",
				__DATE__,__TIME__};
/*****************************************************************
Compare two disk partitions
The assumption is that the source has been copied to the dst
It is also assumed that the diskwipe program was run with
source-fill-byte for the source disk
and dst-fill-byte for the destination disk

Report (in log file) the number of corresponding sectors that are the same and
the number of corresponding sectors that differ by at least one byte.
Two sectors are corresponding if one is on the source partition and
the other sector is in the same relative location on the destination
partition.

If the destination is larger than the source, then each sector of the destination
not corresponding to a source sector is classified as follows:
zero filled
source byte filled
destination byte filled
some other fill byte
other (not filled) sectors

report (in log file) the number of destination sectors in each classification.

Command line:
diskcmp test-case host operator source-disk source-fill-byte dst-disk dst-fill-byte

source and destination disks are specified in hex: e.g., 80
fill bytes are two digit hex values: e.g., B4


High level design:
	decode command line
	open source and dst disks
	for each sector (address) common to both disk partitions
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



/*****************************************************************
get the absolute sector address of the start of a partition
p is the partition table entries list
ix identifies the desired partition table entry
base returns the LBA address of the start of the partition
n returns the number of sectors in the partition
*****************************************************************/
int get_partition_offset (pte_ptr p, int ix, off_t *base, off_t *n) {
	off_t		pt_base;
	int		at = 1,
			i;
	pte_ptr		sub;

	for (i = 0; i < 4; i++) { /* look at each primary PTE (partition table entry) */
		if (at == ix) { /* found desired entry */
			*base = p[i].lba_start;
			*n = p[i].lba_length;
			return 0;
		}
		/* If this is an extended PTE look inside */
		at++;
		sub = p[i].next;
		pt_base = p[i].lba_start; /* get base of extended PTE */

		while(sub) {
			if (at == ix) { /* found the desired entry */
				*base = pt_base + sub->lba_start;
				*n = sub->lba_length;
				return 0;
			}
			at++;
			if (is_extended(sub->type))/* another extended PTE */
				pt_base = (p[i].lba_start + sub->lba_start); /* move base address */
			sub = sub->next;
		}
	}
	return 1;
}

/*****************************************************************
Disk housekeeping that must be done for both source and destination disks
Open disk
get partition table
get base address of desired partition
deal with errors
write record keeping info to log file

drive is the disk to open
caption is either "source" or "destination" (for log file)
log is the log file
p is the command line
ask_for_partitions = true => do dialog to get partition index
px is the partition index (if supplied on command line)
return base -- partition base address (LBA)
return n -- number of sectors in partition
return disk -- disk control pointer to access drive
*****************************************************************/
int setup_disk (char* drive, char *caption, FILE *log, char **p, int ask_for_partitions,
		 int px, off_t *base, off_t *n, disk_control_ptr *disk)
{
	int	status;
	static	pte_rec pt[4];

	*disk = open_disk (drive,&status);
	if (status) {
		printf ("%s could not open drive %s, status code %d\n", p[0], drive, status);
		fprintf (log,"%s could not open drive %s, status code %d\n", p[0], drive, status);
		return 1;
	}
	status = get_partition_table(*disk,pt);
	if (status) {
		printf ("Could not read %s partition table on drive %s\n", caption, drive);
		fprintf (log,"Could not read %s partition table on drive %s\n", caption, drive);
		return 1;
	}
	if (ask_for_partitions) { /* ask if not given on command line */
		print_partition_table(stdout, pt, 1, 1);
		printf("Select partition: ");
		scanf("%d", &px);
		printf("\nPartition %d selected.\n", px);
	}

	status = get_partition_offset (pt, px, base, n);
	if (status) {
		printf ("Could not find %s partition %d\n", caption, px);
		return 1;
	}
	printf ("%s partition %d at %llu for %llu\n", caption, px, *base, *n);
	/* log information about disk and partition */
	log_disk (log, caption, *disk);
	print_partition_table(log, pt, 1, 1);
	fprintf (log,"%s partition %d at %llu for %llu\n", caption, px, *base, *n);
	return 0;
}
 
/*****************************************************************
Print usage and options
*****************************************************************/
void print_help(char *p)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator src-drive src-fill dst-drive dst-fill [-options]\n",p);
	printf ("-select src dst\tSelect partitions to compare\n");
	printf ("-boot\tInclude Boot track in compare\n");
/*	printf ("               \tformat for src & dst P.N,\n");
	printf ("               \twhere: P is primary partition number\n");
	printf ("               \tN is sequence number\n");  */
	printf ("-comment \" ... \"\tDescriptive comment\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is cmpptlog.txt)\n");
	printf ("-h\tPrint this option list\n");
}


/*****************************************************************
Start here
*****************************************************************/
int main(int np, char **p)
{
	char		src_drive[NAME_LENGTH] = "/dev/hda", /* source drive */
			dst_drive[NAME_LENGTH] = "/dev/hdb"; /* destination drive */
	int		help = 0, /* set to true to indicate problem with command line */
			ask_for_partitions = 1,	/* default is true; set to false if partitions given on command line */
			status, /* error return on I/O operations */
			i; /* loop index */
	static disk_control_block *src_disk,*dst_disk;
	static off_t	lba = 0, /* a sector LBA address usually found as a loop index */
			common, /* number of sectors common to source & destination */
			diffs = 0, /* count of sectors that don't match */
			src_lba, /* address of current sector on source */
			dst_lba,  /* address of current sector on destination */
			byte_diffs = 0, /* count of bytes that differ between src and dst */
			match = 0, /* number of matching sectors */
			/* zero .. other apply to dst sectors beyond common area */
			zero = 0, /* number of zero filled sectors */
			sfill = 0, /* number of sectors filled with src fill char */
			dfill = 0, /* number of sectors filled with dst fill char */
			ofill = 0, /* number of sectors filled with some other fill char */
			other = 0; /* number of remaining (unfilled) sectors */
	static unsigned long nz, /* number of zero filled bytes in a sector */
			nfill; /* number of fill bytes in a sector */
	int		big_src = 0, /* true if src bigger than dst */
			big_dst = 0, /* true if dst bigger than src */
			is_diff,
			boot_track_too = 0; /* include boot track in compare */
	int		src_status,
			dst_status; /* I/O error returns */
	static unsigned char *src_buff,*dst_buff; /* sector buffers */
	static time_t	from; /* run start time */
	FILE		*log; /* log file */
	int		is_debug = 0,
			log_diffs = 0;
	unsigned char	src_fill_char,
			dst_fill_char; /* fill characters */
	int		fill_char;
	int		src_px,dst_px; /* partition table indices from command line */
	off_t		src_base, /* LBA address of source partition */
			src_n, /* size (number of sectors in) source partition */
		 	dst_base, /* ditto dst */
			dst_n;
	/* range_ptr is used to track a list of ranges. In this case the ranges
	are disk areas specified in LBA addresses */
	range_ptr	d_r = create_range_list(), /* common area sectors that don't match */
			zf_r = create_range_list(), /* zero filled sectors */
			sf_r = create_range_list(), /* sectors with src-fill */
			df_r = create_range_list(), /* sectors with dst-fill */
			of_r = create_range_list(), /* sectors with some other fill */
			o_r = create_range_list(); /* other (unfilled) sectors */
	static char	comment[NAME_LENGTH] = "",
			log_name[NAME_LENGTH] = "cmpptlog.txt",
			access[2] = "a";

/*****************************************************************
get start run time and decode command line
*****************************************************************/
	time(&from);

	if (np < 8) {
		printf ("%s: Missing parameters\n",p[0]);
		print_help(p[0]);
		return 1;
	}

/*****************************************************************
get fill characters from command line
*****************************************************************/

	sscanf (p[5],"%2x",&fill_char);
	src_fill_char = fill_char;
	sscanf (p[7],"%2x",&fill_char);
	dst_fill_char = fill_char;

/*****************************************************************
get options from command line
*****************************************************************/

	for (i = 8; i < np; i++) {
		if (strcmp (p[i],"-h") == 0) help = 1; /* ask for help */
		else if (strcmp (p[i],"-select") == 0) { /* /select src_ix dst_ix */
			i = i + 2;
			if (i >= np) {
				printf ("%s -select requires two parameters: src partition index\n", p[0]);
				printf ("and dst partition index\n");
				help = 1;
			} else {
				sscanf (p[i-1],"%d",&src_px);
				sscanf (p[i],"%d",&dst_px);
				ask_for_partitions = 0; /* we got 'em so don't ask */
			}
		} else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i], "-log_name") == 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else strncpy(log_name, p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-boot")== 0) boot_track_too = 1;
		else if (strcmp (p[i],"-comment")== 0) {
			i++;
			if ( i>=np){
				printf ("%s: comment required with -comment\n",	p[0]);
				help = 1;
			} else strncpy (comment,p[i], NAME_LENGTH - 1);
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
/*****************************************************************
get source and destination drives (in hex) from command line
*****************************************************************/

	strncpy(src_drive, p[4], NAME_LENGTH - 1);
	strncpy(dst_drive, p[6], NAME_LENGTH - 1);

	if (!strcmp(src_drive, dst_drive)) {
		help = 1;
		printf ("Source and destination drives must be different\n");
	}

/*****************************************************************
If there is a problem on command line, then print help message
*****************************************************************/

	if (help) {
		print_help(p[0]);
		return 0;
	} 

/*****************************************************************
Open log file, source disk and destination disk
*****************************************************************/

	log = log_open (log_name, access, comment, SCCS_ID, np, p);
	status = setup_disk (src_drive, "Source disk", log, p, ask_for_partitions,
		 src_px, &src_base, &src_n, &src_disk);
	status = status || setup_disk (dst_drive, "Destination disk", log, p, ask_for_partitions,
		 dst_px, &dst_base, &dst_n, &dst_disk);
	if (status) return 1;

/*****************************************************************
get ready to do the compare
see which is bigger: src or dst
*****************************************************************/
	if (src_n != dst_n) {
		if ( src_n < dst_n) {
			common = src_n;
			big_dst = 1;
		} else {
			common = dst_n;
			big_src = 1;
		}
	} else common = src_n;
	printf ("Source disk fill byte %2X\n", src_fill_char);
	printf ("Destination disk fill byte %2X\n", dst_fill_char);
	fprintf (log, "Source disk fill byte %2X\n", src_fill_char);
	fprintf (log, "Destination disk fill byte %2X\n", dst_fill_char);
	src_lba = src_base;
	dst_lba = dst_base;
	if (boot_track_too) {
		common += 63;
		src_lba -= 63;
		dst_lba -= 63;
		src_base -= 63;
		dst_base -= 63;
		src_n += 63;
		dst_n += 63;
	}
	fprintf (log,"Source base sector %llu Destination base sector %llu\n",
		src_base,dst_base); 
/*****************************************************************
Main compare loop:
	for each sector in common
		read src sector
		read dst sector
		if match then increment match count
		else increment different count
*****************************************************************/
	for (lba = 0; lba < common; lba++) {
		feedback (from, 0, lba, big_dst ? dst_n : common); /* give progress feedback to user */
		is_diff = 0;
		src_status = read_lba(src_disk, src_lba++, &src_buff);
		dst_status = read_lba(dst_disk, dst_lba++, &dst_buff);
		if (src_status || dst_status) {
			fprintf (log,"read error at sector %llu: src %d dst %d\n", lba, src_status, dst_status);
			printf ("read error at lba %llu: src %d dst %d\n", lba, src_status, dst_status);
			return 1;
		}
/*****************************************************************
Compare corresponding sectors
*****************************************************************/

		for (i = 0; i < BYTES_PER_SECTOR; i++) {
			if (src_buff[i] != dst_buff[i]) {
				is_diff = 1;
				byte_diffs++;
			}
		}
		if (is_diff) {
			diffs++;
			add_to_range (d_r,lba);
			if (log_diffs && (diffs <= 50)) {
				 fprintf (log,"%12llu ",lba);
				 if ((diffs%5) == 0) fprintf (log,"\n");
			}
		}
		else {
			match++;
		}
	}
/*****************************************************************
Log results for corresponding sectors
*****************************************************************/

	if  (log_diffs && (diffs)) fprintf (log,"\n");
	fprintf (log,"Sectors compared: %12llu\n",common);
	fprintf (log,"Sectors match:    %12llu\n",match);
	fprintf (log,"Sectors differ:   %12llu\n",diffs);
	fprintf (log,"Bytes differ:     %12llu\n",byte_diffs);
	print_range_list(log,"Diffs range: ",d_r);
	if (big_src) {
		fprintf (log,"Source (%llu) has %llu more sectors than destination (%llu)\n", src_n, src_n - dst_n, dst_n);
	}
/*****************************************************************
If the destination is larger than the source then
	look at the remainder of the destination
*****************************************************************/
	else if (big_dst) {
		fprintf (log,"Source (%llu) has %llu fewer sectors than destination (%llu)\n", src_n, dst_n - src_n, dst_n);
		zero = 0;
		ofill = sfill = dfill = 0;
		other = 0;
		printf ("Destination larger than source; scanning %llu sectors\n", dst_n-common);
		for (lba = common; lba < dst_n; is_debug?(lba+=100):lba++){
			feedback (from, 0, lba, dst_n);
			if((dst_status = read_lba(dst_disk, dst_lba++, &dst_buff))) {
				fprintf (log,"read error at sector %llu: dst %d\n", lba, dst_status);
				printf ("read error at lba %llu: dst %d\n", lba, dst_status);
			}
			nz = 0;
			nfill = 0; 
/*****************************************************************
classify sector: count zero bytes and fill bytes
how to count fill bytes? the rule is: all bytes after
byte [23] are the same. i.e., 488 bytes of the sector are the
same. We use 480 to give some slack.
*****************************************************************/

			for (i = 0; i < BYTES_PER_SECTOR; i++) {
				if ( dst_buff[i] == 0) nz++;
				else if (dst_buff[i] == dst_buff[BUFF_OFF]) nfill++;
			}
			if (nz == BYTES_PER_SECTOR) { zero++; add_to_range(zf_r,lba); }
			else if (nfill > 480) {
					if (dst_buff[BUFF_OFF] == src_fill_char) {
						sfill++;
						add_to_range(sf_r,lba);
					} else if (dst_buff[BUFF_OFF] == dst_fill_char) {
						dfill++;
						add_to_range(df_r,lba);
					} else {
						ofill++;
						add_to_range(of_r,lba);
					}
			}
			else {
				other++;
				add_to_range (o_r,lba);
			}
		}
/*****************************************************************
log results to log file
*****************************************************************/
		if  (log_diffs && (other)) fprintf (log, "\n");
		fprintf (log,"Zero fill:     %llu\n", zero);
		fprintf (log,"Src Byte fill (%02X): %llu\n", src_fill_char, sfill);
		if (src_fill_char == dst_fill_char)
			fprintf (log, "Dst Fill Byte same as Src Fill Byte\n");
		else fprintf (log, "Dst Byte fill (%02X): %lu\n", dst_fill_char, dfill);
		fprintf (log,"Other fill:    %llu\n", ofill);
		fprintf (log,"Other no fill: %llu\n", other);
		print_range_list(log,"Zero fill range: ", zf_r);
		print_range_list(log,"Src fill range: ", sf_r);
		print_range_list(log,"Dst fill range: ", df_r);
		print_range_list(log,"Other fill range: ", of_r);
		print_range_list(log,"Other not filled range: ", o_r);
	}
	log_close(log, from);
	return 0;
}
