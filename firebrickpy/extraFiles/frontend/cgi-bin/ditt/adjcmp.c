static char *SCCS_ID[] = {"@(#) adjcmp.c Linux Version 1.4 Created 03/25/05 at 19:16:24",
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
/* Modified by Kelsey Rider, NIST/SDCT Sept 2004 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include "zbios.h"
# include <malloc.h>
# include <time.h>

/******************************************************************************
Compare two disks by partitions: ADJCMP
The ADJCMP program is used to compare corresponding sectors of two disks where
the corresponding sectors are located at different addresses on the source disk
and the destination disk. Some disk imaging tools optionally create a
destination with partitions aligned to disk cylinder boundaries. All the
content of the source disk is reproduced on the destination, just not at the
same location as on the source disk.
ADJCMP divides the source and destination into disk chunks. Where a disk
chunk is defined as a group of contiguous sectors as described below.
Corresponding disk chunks between the source and destination are identified and
sectors at the same location relative to the first sector of the corresponding
chunk are compared.  A disk chunk is one of the following:
· Partition defined in the partition table
· Partition boot track (the track just before the start of the partition)
· Unallocated sectors (a group of contiguous sectors not part of any partition)
ADJCMP automatically designates disk chunks found in the same order on the
source and destination as corresponding. The user may optionally do the
assignments by an interactive dialog.
A comparison report is logged for each disk chunk.

ADJCMP command line
adjcmp case host operator src-drv src-fill dst-drv dst-fill [/opts]

Parameter	Description
case			A test case identifier
host			The name of the computer running the test
operator		The initials of the person running the test
src-drv		The drive number in hex. Possible values begin at 80. The drive
				where the source drive is mounted.
src-fill		A two digit hex value, used to fill each sector.
dst-drv		The drive number in hex. Possible values begin at 80. The drive
				where the destination drive is mounted.
dst-fill		A two digit hex value, used to fill each sector.
-comment "…"	A comment for the test log file. If this option is not used,
				the program prompts for a log floppy to be inserted. If this option
				is used, then a log floppy is assumed to be already loaded.
-assign		Engage the user in a dialog to assign corresponding regions of the
				source and destination disks for comparison.
-h				Print a summary of command options and exit


Program outline:
get command line
layout the source disk (identify the chunks)
layout the destination disk (identify the chunks)
assign corresponding chunks
compare source to destination (chunk by chunk)
log results

Disk layout (find all the chunks):
let n = # of sectors on disk
set at = 0
while (at < n){
	if (there is a partition that starts with "at") {
		make the partition a chunk, set "at" to partition end + 1
	}
	else {
		find first partition (or the end of the disk) that starts
		after "at". Make the space from "at" up the partition a chunk.
		set "at" to the partition start (or end of disk)
	}
}


******************************************************************************/



typedef struct {
	pte_ptr		pt;
	char		chunk_class; /* partition, boot track, unalloc, fill */
	off_t 		lba_start, /* starting address of the chunk */
			n_sectors; /* size of the chunk */
} layout_rec, *layout_ptr;

typedef struct { /* summary totals */
	off_t	boot_track_diffs;	/* boot track total diffs */
	off_t	partition_diffs;	/* total diffs in partitions */
	off_t	unalloc_diffs;		/* total diffs in unallocated chunks */
	off_t	fill_zero;		/* partition fill area zero filled */
	off_t	fill_non_zero;		/* partition fill area not zero */
	off_t	excess_zero;		/* unallocated areas zero filled */
	off_t	excess_non_zero;	/* unallocated areas not zero */
	off_t	n_common;		/* number of sectors common to src & dst */
	off_t	n_common_unalloc;	/* number of common unallocated sectors */
	int	n_boot_tracks;		/* number of boot tracks */
	int	n_partitions;		/* number of partitions */
	int	n_unalloc;		/* number of unallocated chunks */
} totals_rec, *totals_ptr;


/******************************************************************************
Examine a part of the destination disk that does not correspond to any area
on the source disk. This is either (1) the sectors of a destination chunk that
do not correspond to the source sectors of the corresponding chunk. or
(2) destination chunk of sectors not allocated to a corresponding source chunk.
To say this another way, source partitions are compared to destination partitions
that are a little larger. Sectors in (1) above are the excess destination sectors.
If the source has unallocated (i.e., not in a partition) sectors between two
partitions, the area between is treated as a partition. Sectors in (2) are the
excess sectors after the last partition on the destination.
******************************************************************************/
void scan_region (FILE *log, /* the log file */
	disk_control_ptr dst_disk, /* the destination disk */
	off_t		common,  /* starting sector LBA address */
	off_t		dst_n,   /* number of sectors to scan */
	unsigned char	src_fill_char, /* the source fill character */
	unsigned char	dst_fill_char, /* the destination fill character */
	time_t		start_time, /* time the program started running */
	off_t		*tz, /* update value: running total of zero fill sectors */
	off_t		*tnz) /* update value: running total of non-zero sectors */
{
	range_ptr	zf_r = create_range_list(), /* range of zero fill sectors */
			sf_r = create_range_list(), /* range of source fill sectors */
			df_r = create_range_list(), /* range of destination fill sectors */
			of_r = create_range_list(), /* range of other fill sectors */
			o_r = create_range_list(); /* range of other sectors */
	unsigned char	*dst_buff, /* the sector to scan */
			other_fill_char = 0; /* last other fill char seen */
	int		dst_status; /* disk I/O status return */
	off_t		lba = 0, /* the sector relative to start address */
			nz,  /* count of zero bytes in a sector */
			nfill, /* count of fill bytes in a sector */
			dst_lba, /* the absolute lba of sector to examine */
			zero = 0, /* number of zero sectors */
			sfill = 0, /* number of sectors filled with source byte */
			dfill = 0, /* number of sectors filled with dst byte */
			ofill = 0, /* number of sectors filled with something else */
			other = 0; /* count of other sectors */
	int		i, /* look index */
			other_fill_seen = 0, /* flag indicating fill other than src/dst */
			new_fill = 0;

	printf ("scanning %llu unmatched sectors: %llu--%llu\n",dst_n-common,common,dst_n);
	fprintf (log,"scanning %llu unmatched sectors: %llu--%llu\n",dst_n-common,common,dst_n);

/******************************************************************************
Loop to scan dst_n sectors from common up to common + dst_n
******************************************************************************/
	dst_lba = common;
	for (lba = common; lba < dst_n; lba++) {
		feedback (start_time,0,dst_lba,n_sectors(dst_disk));
		dst_status = read_lba(dst_disk,dst_lba++,&dst_buff);
		if (dst_status) {
			fprintf (log,"dst read error 0x%02X on track starting at lba %llu\n",dst_status,dst_lba-1);
			printf ("dst read error 0x%02X on track starting at lba %llu\n",dst_status,dst_lba-1);
			exit(1);
		}
		nz = 0;
		nfill = 0;
/******************************************************************************
scan the sector, count number of zero bytes and number of fill bytes.
To count fill bytes: assume sector is filled (from diskwipe) then ...
bytes 1-27 has the sector address and the remaining bytes are the same.
so pick byte # 30 and count the number of bytes that match dst_buff[30],
if enough match (480) then call it filled. The magic constants 30 and 480
allow some room for diskwipe to be off by a few bytes.
******************************************************************************/
		for (i = 0; i < BYTES_PER_SECTOR; i++) {
			if ( dst_buff[i] == 0) nz++;
			else if (dst_buff[i] == dst_buff[BUFF_OFF]) nfill++;
		}
		if (nz == BYTES_PER_SECTOR) { zero++; add_to_range(zf_r,lba); } /* zero sector */
		else if ((nfill > 480) && (dst_buff[BUFF_OFF] != 0x00)) { /* filled sector */
			if (dst_buff[BUFF_OFF] == src_fill_char) { /* src fill */
				sfill++;
				add_to_range(sf_r,lba);
			} else if (dst_buff[BUFF_OFF] == dst_fill_char) { /* dst fill */
				dfill++;
				add_to_range(df_r,lba);
			} else { /* filled with something other than src or dst!! */
				ofill++;
				add_to_range(of_r,lba);
				if (other_fill_seen) {
					if (dst_buff[BUFF_OFF] != other_fill_char) new_fill = 1;
				} else {  /* remember the other fill char */
					other_fill_char = dst_buff[BUFF_OFF];
					new_fill = 0;
				}
			}
		} else {
			other++; /* not zero and not filled */
			add_to_range (o_r,lba);
		}
	}
/******************************************************************************
Log results
******************************************************************************/
	fprintf (log,"Zero fill:           %llu\n",zero);
	fprintf (log,"Src Byte fill (%02X): %llu\n",src_fill_char,sfill);
	if (src_fill_char == dst_fill_char )
		fprintf (log,"Dst Fill Byte same as Src Fill Byte\n");
	else fprintf (log,"Dst Byte fill (%02X): %llu\n",dst_fill_char,dfill);
	fprintf (log,"Other fill   %c(%02X): %llu\n",new_fill?'+':' ',
		other_fill_char,ofill);
	fprintf (log,"Other no fill:        %llu\n",other);
	print_range_list (log,"Zero fill range: ",zf_r);
	print_range_list (log,"Src fill range: ",sf_r);
	print_range_list (log,"Dst fill range: ",df_r);
	print_range_list (log,"Other fill range: ",of_r);
	print_range_list (log,"Other not filled range: ",o_r);

/******************************************************************************
Update running totals for summary
******************************************************************************/
	*tz = *tz + zero;
	*tnz = *tnz + sfill + dfill + ofill + other;
}

/******************************************************************************
Compare a source chunk to a destination chunk
log number of sectors compared, # match, # diff, etc
If dst is larger, (almost certain) then run scan_region on excess sectors
******************************************************************************/
int cmp_region (FILE *log, /* the log file */
	/* source parameters: disk, chunk description, fill char */
	disk_control_ptr src_disk, layout_ptr src, char src_fill,
	/* destination parameters: disk, chunk description, fill char */
	disk_control_ptr dst_disk, layout_ptr dst, char dst_fill,
	time_t start_time, /* time the program started running (for user feedback) */
	totals_ptr t) /* summary totals */
{
	unsigned char	*src_buff,
			*dst_buff; /* sector read buffers */
	int		src_status,
			dst_status;  /* disk read status return */
	off_t		lba = 0, /* index for main loop; relative sector in partition */
			common, /* number of sectors with both a src and dst sector */
			diffs = 0, /* number of sectors that differ */
			src_lba,  /* absolute LBA of src sector */
			dst_lba,  /* absolute LBA of dst sector */
			byte_diffs = 0, /* number of bytes that differ */
			match = 0; /* number of sectors that match */
	int		big_src = 0, /* src is bigger than dst */
			big_dst = 0, /* dst is bigger than src */
			is_diff, /* src and dst do not match */
			i; /* loop index */
	range_ptr	d_r = create_range_list(); /* sectors that do not match */

	if (src->n_sectors == dst->n_sectors) common = src->n_sectors;
	else if (src->n_sectors > dst->n_sectors) {
		common = dst->n_sectors;
		big_src = 1;
	} else {
		common = src->n_sectors;
		big_dst = 1;
	}
	src_lba = src->lba_start;
	dst_lba = dst->lba_start;
	fprintf (log,"Src base %llu Dst base %llu\n",
		src_lba,dst_lba);
	for (lba = 0; lba < common;lba++) { /* main loop: scan sectors that correspond */
		is_diff = 0;
		feedback(start_time,0,dst_lba,n_sectors(dst_disk));
		src_status = read_lba(src_disk,src_lba++,&src_buff);
		dst_status = read_lba(dst_disk,dst_lba++,&dst_buff);
		if (src_status) {
			fprintf (log,"src read error 0x%02X on track starting at lba %llu\n",src_status,lba);
			printf ("src read error 0x%02X on track starting at lba %llu\n",src_status,lba);
		}
		if (dst_status) {
			fprintf (log,"dst read error 0x%02X on track starting at lba %llu\n",dst_status,lba);
			printf ("dst read error 0x%02X on track starting at lba %llu\n",dst_status,lba);
		}
		if(src_status || dst_status) return 1;

		/* scan the sectors; note any diffs */
		for (i = 0; i < BYTES_PER_SECTOR; i++) {
			if (src_buff[i] != dst_buff[i]) {
				is_diff = 1;
				byte_diffs++;
			}
		}
		if (is_diff) { /* rats! not a match */
			diffs++;
			add_to_range (d_r,lba);
		} else { /* the source and dst are the same */
			match++;
		}
	} /* log results */
	fprintf (log,"Sectors compared: %12llu\n",common);
	fprintf (log,"Sectors match:    %12llu\n",match);
	fprintf (log,"Sectors differ:   %12llu\n",diffs);
	fprintf (log,"Bytes differ:     %12llu\n",byte_diffs);
	print_range_list(log,"Diffs range: ",d_r);
	if (big_src) {
		fprintf (log,"Source (%llu) has %llu more sectors than destination (%llu)\n",
			src->n_sectors,src->n_sectors - dst->n_sectors,
			dst->n_sectors);
	} else if (big_dst) { /* dst has more sectors to examine */
		fprintf (log,"Source (%llu) has %llu fewer sectors than destination (%llu)\n",
			src->n_sectors,dst->n_sectors - src->n_sectors,
			dst->n_sectors);
		printf ("Source (%llu) has %llu fewer sectors than destination (%llu)\n",
			src->n_sectors,dst->n_sectors - src->n_sectors,
			dst->n_sectors);
		if (dst->chunk_class == CHUNK_UNALLOCATED) /* look at excess sectors in chunk */
			scan_region (log, dst_disk,dst_lba,dst->lba_start + dst->n_sectors,
				src_fill, dst_fill,start_time,
				&t->excess_zero,&t->excess_non_zero);
		else scan_region (log, dst_disk,dst_lba,dst->lba_start + dst->n_sectors,
				src_fill, dst_fill,start_time,
				&t->fill_zero,&t->fill_non_zero);
	}
/******************************************************************************
Update summary totals
******************************************************************************/
	if (dst->chunk_class == CHUNK_PARTITION) { /* partition summary */
		t->n_partitions++;
		t->partition_diffs += diffs;
		t->n_common += common;
	} else if (dst->chunk_class == CHUNK_UNALLOCATED) { /* nothing to do for unallocated chunk */ 
		t->n_unalloc++;
		t->unalloc_diffs += diffs;
		t->n_common_unalloc += common;
	} else { /* boot track summary */
		t->n_boot_tracks++;
		t->boot_track_diffs += diffs;
	}
	return 0;
}

/******************************************************************************
Compare the source to the destination
Assign corresponding regions automatically
if user requests, allow user to edit assignments
For each chunk
	cmp_region
If the destination has any excess sectors: scan_region
Log summary results
******************************************************************************/
int do_compare (
	/* src parameters: disk, chunks */
	disk_control_ptr src_dcb, int src_n_regions, layout_ptr src_layout,
	/* dst parameters: disk, chunks */
	disk_control_ptr dst_dcb, int dst_n_regions, layout_ptr dst_layout,
	/* fill characters */
	unsigned char src_fill, unsigned char dst_fill,
	FILE *log, /* log file */
	int do_dialog, /* user requests to assign corresponding chunks */
	time_t start_time) /* time the program started running */
{
	int		nm = 0,
			num = 0,
			ix,
			i,
			j,
			more = 1,
			status = 0;
	static int	ml[2*MAX_PARTITIONS],
			uml[2*MAX_PARTITIONS];
	char		ans[20];
	static totals_rec t = {0L,0L,0L,0L,0L,0L,0L,0L,0L,0,0,0};

	if (src_n_regions <= dst_n_regions) {
		nm = src_n_regions;
		for (i = 0; i < nm; i++) {uml[i] = i; ml[i] = i;}
		for (i = nm; i < dst_n_regions;i++) uml[i] = -1;
	} else {
		nm = dst_n_regions;
		for (i = 0; i < nm; i++) {uml[i] = i; ml[i] = i;}
		for (i = nm; i < src_n_regions;i++) ml[i] = 0;
	}

/******************************************************************************
Assign corresponding regions:
default assignment is to assume src and dst have same layout except dst may
have an extra chnuk of unallocated space at the end.
******************************************************************************/
	while (more) {
		printf ("Matching regions\n%4s%10s%10s%10s%9s%10s%10s%10s\n",
		"","Start","End","Length","","Start","End","Length");
		for (i = 0; i < src_n_regions; i++) {
			printf ("%2d %c %9llu %9llu %8llu => ",
				i,
				src_layout[i].chunk_class,
				src_layout[i].lba_start,
				src_layout[i].lba_start + src_layout[i].n_sectors - 1,
				src_layout[i].n_sectors);
			j = ml[i];
			printf ("%2d %c %9llu %9llu %8llu\n",
				j,
				dst_layout[j].chunk_class,
				dst_layout[j].lba_start,
				dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
				dst_layout[j].n_sectors);
		}
		if (do_dialog) { /* if user requested a dialog, ask if OK */
			printf ("Assignment correct? [y/n] (default: n)\n");
			scanf ("%s", ans);
			printf ("ans %s\n",ans);
		} else ans[0] = 'y';
		if (ans[0] == 'y') more = 0;
		else { /* user wants to assign chunks */
			for (i = 0; i < dst_n_regions; i++) uml[i] = -1; /* mark dst chunks free */
			printf ("\n\nMatch regions: type # of matching dst region or -1 to print list\n");
			for (i = 0; i < src_n_regions; i++){
				printf ("Enter match for: "); /* Get a match for each src chunk */
				printf ("%2d %c from %llu to %llu len=%llu %8.2fMB %8.2fBMB\n",
					i,
					src_layout[i].chunk_class,
					src_layout[i].lba_start,
					src_layout[i].lba_start + src_layout[i].n_sectors - 1,
					src_layout[i].n_sectors,
					BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1000000.0,
					BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1048576.0);
				scanf ("%d",&ix);
				while (ix == -1 || ix > 49){ /* -1 => print list of dst chunks; 50 > size of arrays */
					for (j = 0; j < dst_n_regions; j++){
						if (uml[j] == -1) printf ("ok "); /* available for selection */
						else printf ("NO "); /* already assigned */
						printf ("%2d %c %9llu %9llu %9llu %8.2fMB %8.2fBMB\n",
							j,
							dst_layout[j].chunk_class,
							dst_layout[j].lba_start,
							dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
							dst_layout[j].n_sectors,
							BYTES_PER_SECTOR*(float)dst_layout[j].n_sectors/1000000.0,
							BYTES_PER_SECTOR*(float)dst_layout[j].n_sectors/1048576.0);
					}
					printf ("Enter match for: %d ",i);
					scanf("%d",&ix);
				}

				ml[i] = ix;
				if (uml[ix] != -1) /* dst chunk already assigned to src chunk */
					printf ("Warning: dst region %d assigned to src region %d (reassigning)\n",
						ix, uml[ix]);
				uml[ix] = i;

			}
			nm = src_n_regions;
		}
	}
/******************************************************************************
Log chunk assignments
******************************************************************************/
		fprintf (log,"Matching regions\n");
		fprintf (log,"%4s%10s%10s%8s%8s%10s%10s%9s\n",
		"","Start","End","Length","","Start","End","Length");
		for (i = 0; i < src_n_regions; i++) {
			fprintf (log,"%2d %c %9llu %9llu %8llu => ",
				i,
				src_layout[i].chunk_class,
				src_layout[i].lba_start,
				src_layout[i].lba_start + src_layout[i].n_sectors - 1,
				src_layout[i].n_sectors);
			j = ml[i];
			fprintf (log,"%2d %c %9llu %9llu %8llu\n",
				j,
				dst_layout[j].chunk_class,
				dst_layout[j].lba_start,
				dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
				dst_layout[j].n_sectors);
		}
		fprintf (log,"Unmatched destination regions\n");
		fprintf (log,"%5s%10s%10s%10s\n",
			"","Start","End","Length");
		for (j = 0; j < dst_n_regions; j++) {
			if (uml[j] == -1) {
				fprintf (log,"%2d%c %9llu %9llu %8llu\n",
					j,
					dst_layout[j].chunk_class,
					dst_layout[j].lba_start,
					dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
					dst_layout[j].n_sectors);
				num++;
			}
		}
/******************************************************************************
For each chunk, compare src to dst
******************************************************************************/
		fprintf (log, "Chunk class codes: %c/%c Boot track, %c partition, %c unallocated\n",
			CHUNK_BOOT_EXT, CHUNK_BOOT, CHUNK_PARTITION, CHUNK_UNALLOCATED);
		for (i = 0; i < nm; i++) {
			fprintf (log,"\n===========================================\n");
			fprintf (log,"Compare region %d of %d: src(%llu,%llu,%c) dst (%llu,%llu,%c)\n",
				i, nm-1,
				src_layout[i].lba_start,
				src_layout[i].n_sectors,
				src_layout[i].chunk_class,
				dst_layout[ml[i]].lba_start,
				dst_layout[ml[i]].n_sectors,
				dst_layout[ml[i]].chunk_class);
			printf ("\nCompare region %d of %d: src(%llu,%llu) dst (%llu,%llu)\n",
				i, nm-1,
				src_layout[i].lba_start,
				src_layout[i].n_sectors,
				dst_layout[ml[i]].lba_start,
				dst_layout[ml[i]].n_sectors);
			if (status = cmp_region (log,src_dcb,&src_layout[i],src_fill,
				dst_dcb,&dst_layout[ml[i]],dst_fill,start_time,&t)) return status;
		}
		if (num) {
/******************************************************************************
For each chunk of the destination that is not assigned to a src chunk
	examine the chunk for
		zero sectors
		filled sectors
		other sectors
	and count the number of each class of sector content
******************************************************************************/
			fprintf (log,"\nExamine unmatched regions of destination\n");
			printf ("\nExamine unmatched regions of destination\n");
			for (j = 0; j < dst_n_regions; j++) {
				if (uml[j] == -1) {
					printf ("Examine: %2d%c %9llu %9llu %8llu\n",
						j,
						dst_layout[j].chunk_class,
						dst_layout[j].lba_start,
						dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
						dst_layout[j].n_sectors);
					fprintf (log,"\n===========================================\n");
					fprintf (log,"Examine: %2d%c %9llu--%9llu %8llu\n",
						j,
						dst_layout[j].chunk_class,
						dst_layout[j].lba_start,
						dst_layout[j].lba_start + dst_layout[j].n_sectors - 1,
						dst_layout[j].n_sectors);
					scan_region (log,dst_dcb,dst_layout[j].lba_start,
						dst_layout[j].lba_start + dst_layout[j].n_sectors,src_fill,
						dst_fill,start_time,&t.excess_zero,&t.excess_non_zero);
				}
			}
		}

/******************************************************************************
Log summary results
******************************************************************************/
	/*
	fprintf (log,"Boot track summary: %d boot tracks with %ld diffs\n",
		t.n_boot_tracks,t.boot_track_diffs);
	fprintf (log,"Partition summary: %d partitions %ld diffs\n",
		t.n_partitions,t.partition_diffs);
	fprintf (log,"Partition fill area summary: %ld zero %ld other\n",
		t.fill_zero,t.fill_non_zero);
	fprintf (log,"Excess summary: %ld zero %ld other\n",
		t.excess_zero,t.excess_non_zero);  */
	fprintf (log,"\nSummary\n");
	fprintf (log,"Boot tracks %2d    %10d diffs %10llu\n", t.n_boot_tracks,
		63*t.n_boot_tracks, t.boot_track_diffs);
	fprintf (log,"Partitions  %2d    %10llu diffs %10llu\n", t.n_partitions,
		t.n_common, t.partition_diffs);
	fprintf (log,"Unallocated %2d    %10llu diffs %10llu\n", t.n_unalloc,
		t.n_common_unalloc, t.unalloc_diffs);
	fprintf (log,"Total src sectors %10llu\n", 63*t.n_boot_tracks +
		t.n_common_unalloc + t.n_common);
	fprintf (log,"Partition excess  %10llu zero %10llu non-zero %10llu\n",
		t.fill_zero+t.fill_non_zero, t.fill_zero, t.fill_non_zero);
	fprintf (log,"Disk excess       %10llu zero %10llu non-zero %10llu\n",
		t.excess_zero+t.excess_non_zero, t.excess_zero, t.excess_non_zero);
	fprintf (log,"Total dst sectors %10llu\n\n",
		63*t.n_boot_tracks + t.n_common + t.n_common_unalloc +
		t.fill_zero+t.fill_non_zero + t.excess_zero+t.excess_non_zero);

	return 0;
}

/******************************************************************************
Find the lowest-lba partition that starts at LBA max or later
******************************************************************************/
pte_ptr find_min(pte_ptr pt, /* The partition table*/
		off_t max, /* target to find */
		off_t *lba) /* start of next partition */
{
	int	i,
		ix;
	off_t	min = MAX_OFF_T;
	pte_ptr at = NULL,
		sub;
	off_t	offset,
		start;

	*lba = 0;
	for (i = 0; i < 4; i++) {
		if (pt[i].type) {
			if (is_extended(pt[i].type)) {
				offset = 0;
				sub = &pt[i];
				ix = 0;
				start = sub->lba_start;
				while (sub) {
					if(sub->type) {
						if (start >= max) {
							if (min == MAX_OFF_T) {
								min = start;
								*lba = start;
								at = sub;
							} else if (start < min) {
								min = start;
								*lba = start;
								at = sub;
							}
						}
					}
					if (is_extended(sub->type)) { 
						if(ix) offset = sub->lba_start;
					} else { offset = 0; }
					sub = sub->next;
					ix = 1;
					if(sub)start = pt[i].lba_start + offset + sub->lba_start;
				}
			} else {
				if (pt[i].lba_start >= max) {
					if (min == MAX_OFF_T) {
						min = pt[i].lba_start;
						*lba = pt[i].lba_start;
						at = &pt[i];
					} else if (pt[i].lba_start < min) {
						min = pt[i].lba_start;
						at = &pt[i];
						*lba = pt[i].lba_start;
					}
				}
			}
		}
	}
	return at;
}

/******************************************************************************
Examine the disk partition table and return a list of chunks
A chunk is a block of sectors that is either
(1) a partition
(2) a partition boot track
(3) the block of sectors between two partitions or
(4) the block of sectors after the last partition upto the end of the disk
******************************************************************************/
int layout_disk (pte_ptr pt, /* the partition table */
		layout_ptr lp, /* the disk layout to return */
		disk_control_ptr d)/* the disk containing the partition table */
{
	int		n = 0;
	int		more = 1;
	pte_ptr		min_entry;
	off_t		min = 0,
			alloc = 0,
			at;

	do {
		min_entry = find_min (pt, min, &at);
		if (min_entry) {
			if (alloc < at) {
				lp[n].pt = NULL;
				lp[n].lba_start = alloc;
				lp[n].n_sectors = at /*min_entry -> lba_start*/ - alloc;
				if (n) lp[n].chunk_class = CHUNK_UNALLOCATED;
				else lp[n].chunk_class = CHUNK_BOOT;
				n++;
				if(n >= MAX_PARTITIONS) {
					printf("Error: maximum number of partitions (%d) exceeded.\n", MAX_PARTITIONS);
					exit(1);
				}
			}
			lp[n].pt = min_entry;
			lp[n].lba_start = at; /*min_entry -> lba_start*/
			if (is_extended(min_entry->type)) {
				lp[n].n_sectors = DISK_MAX_SECTORS;
				lp[n].chunk_class = CHUNK_BOOT_EXT;
			} else {
				lp[n].chunk_class = CHUNK_PARTITION;
				lp[n].n_sectors = min_entry -> lba_length;
			}
			min = lp[n].lba_start + lp[n].n_sectors;
			alloc = min;
			n++;
		} else more = 0;
	} while (more);
	if (alloc < n_sectors(d)) {
		lp[n].pt = NULL;
		lp[n].chunk_class = CHUNK_UNALLOCATED;
		lp[n].lba_start = alloc;
		lp[n].n_sectors = n_sectors(d) - alloc;
		n++;
	}
	return n;
}

/******************************************************************************
Print the usage and options help
******************************************************************************/
void print_help(char *p /* the program name */)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator src-drive src-fill dst-drive dst-fill [-options]\n",p);
	printf ("-comment \" ... \"\tDescriptive comment\n");
	printf ("-layout\tPrint disk layout only (no compare)\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is cmpalog.txt)\n");
	printf ("-assign \tAssign corresponding regions between src and dst via dialog\n");
	printf ("-h\tPrint this option list\n");
}

main (int np, char **p)
{
	int		help = 0,/* request command line help */
			layout_only = 0; /* print disk layout only */
	int		status,/* disk open or I/O status return */
			i; /* loop index */

	/* variables to handle fill characters */
	unsigned char	src_fill = 'S',
			dst_fill = 'D';
	int		is_fill,
			id_fill;

	/* source disk parameters */
	static disk_control_ptr src_dcb; /* drive information */
	char		src_drive[NAME_LENGTH] = "/dev/hda"; /* source drive, default is Master on Primary IDE */
	pte_rec		src_pt[4]; /* partition table for source disk */
	int		src_n_regions = 0; /* number of chunks on the source */
	layout_ptr	src_layout = (layout_ptr) malloc(MAX_PARTITIONS*sizeof(layout_rec)); /* chunks */
   
	/* destination disk parameters */
	static disk_control_ptr dst_dcb;
	char		dst_drive[NAME_LENGTH] = "/dev/hdb";
	pte_rec		dst_pt[4];
	int		dst_n_regions = 0;
	layout_ptr	dst_layout = (layout_ptr) malloc(MAX_PARTITIONS*sizeof(layout_rec));

	FILE		*log; /* log file */
	char		comment [NAME_LENGTH] = "",
			log_name[NAME_LENGTH] = "cmpalog.txt",
			access[2] = "a"; /* tester (user) comment for log file */
	static time_t	from; /* time program started running */
	int		assign_regions = 0; /* flag: user wants to assign corresponding chunks */

/*	_stklen = 2*_stklen; */

	printf ("\n%s Version 3.1 compiled at %s on %s\n", p[0],
		__TIME__,__DATE__);
/******************************************************************************
Get command line
******************************************************************************/
	if (np < 8) {
		printf ("%s: Missing parameters\n",p[0]);
		help = 1;
	} else {
		sscanf (p[5],"%2x",&is_fill);
		src_fill = is_fill;
		sscanf (p[7],"%2x",&id_fill);
		dst_fill = id_fill;
		strncpy(src_drive, p[4], NAME_LENGTH - 1);
		strncpy(dst_drive, p[6], NAME_LENGTH - 1);
		printf ("Src drive %s dst drive %s\n",src_drive,dst_drive);
		printf ("Src fill 0x%02X dst fill 0x%02X\n",src_fill,dst_fill);
	}

	for (i = 8; i < np; i++) {
		if (strcmp(p[i], "-assign") == 0) assign_regions = 1;
		else if (strcmp (p[i], "-h") == 0) help = 1;
		else if (strcmp (p[i], "-layout") == 0) layout_only = 1;
		else if (strcmp (p[i], "-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i], "-log_name") == 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else strncpy(log_name, p[i], NAME_LENGTH - 1);
		} else if (strcmp (p[i],"-comment")== 0) {
			i++;
			if (i >= np) {
				printf ("%s: comment required with -comment\n",	p[0]);
				print_help(p[0]);
				return 1;
			}
			strncpy (comment,p[i], NAME_LENGTH - 1);
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}
	if (help) {
		print_help(p[0]);
		return 0;
	}

/******************************************************************************
Start log file, open source disk and get partition table
******************************************************************************/
	log = log_open (log_name, access, comment, SCCS_ID, np, p);
	fprintf (log, "Src drive %s dst drive %s\n",src_drive,dst_drive);
	fprintf (log, "Src fill 0x%02X dst fill 0x%02X\n",src_fill,dst_fill);

	src_dcb = open_disk (src_drive,&status);
	if (status) {
		printf ("Could not access source drive %s status code %d\n",src_drive,status);
		return 1;
	}
	log_disk (log,"Source Disk",src_dcb);
	status = get_partition_table(src_dcb,src_pt);

	if (status == 0) {
		fprintf (log,"Source disk partition table\n");
		print_partition_table(log,src_pt,0,1);
		print_partition_table(stdout,src_pt,1,1);
	} else {
		fprintf (log,"Error reading src partition table code %d\n", status);
		printf ("No partition table signature or error reading partition table (code %d)\n",status);
		return 1;
	}
	time(&from);

	src_n_regions = layout_disk (src_pt,src_layout,src_dcb);
	printf ("%d regions\n",src_n_regions);
	fprintf (log,"Source disk layout: ");
	fprintf (log," %05llu/%03llu/%02llu ",
		src_dcb->disk_max.cylinder,
		src_dcb->disk_max.head,
		src_dcb->disk_max.sector);
	fprintf (log,"%llu total sectors on disk\n",src_dcb->n_sectors);
	fprintf (log,"%4s%10s%10s%10s%23s\n","","Start LBA","End LBA","Length",
		"Size: MB   (binary)");
	for (i = 0; i < src_n_regions; i++){
		printf ("%2d %c %9llu %9llu %9llu %8.2fMB %8.2fBMB\n",
			i,
			src_layout[i].chunk_class,
			src_layout[i].lba_start,
			src_layout[i].lba_start + src_layout[i].n_sectors - 1,
			src_layout[i].n_sectors,
			BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1000000.0,
			BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1048576.0);
		fprintf (log,"%2d %c %9llu %9llu %9llu %8.2fMB %8.2fBMB\n",
			i,
			src_layout[i].chunk_class,
			src_layout[i].lba_start,
			src_layout[i].lba_start + src_layout[i].n_sectors - 1,
			src_layout[i].n_sectors,
			BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1000000.0,
			BYTES_PER_SECTOR*(float)src_layout[i].n_sectors/1048576.0);
	}

/******************************************************************************
Open destination disk, get partition table
******************************************************************************/
	dst_dcb = open_disk (dst_drive,&status);
	if (status) {
		printf ("Could not access destination drive %s status code %d\n",dst_drive,status);
		return 1;
	}
	log_disk (log,"Destination Disk",dst_dcb);
	status = get_partition_table(dst_dcb,dst_pt);
	if (status == 0){
		fprintf (log,"Destination disk partition table\n");
		print_partition_table(log,dst_pt,0,1);
		print_partition_table(stdout,dst_pt,1,1);
	} else {
		fprintf (log,"Error reading src partition table code %d\n", status);
		printf ("No partition table signature or error reading partition table (code %d)\n",status);
		return 1;
	}

	dst_n_regions = layout_disk (dst_pt,dst_layout,dst_dcb);
	printf ("%d regions on %s\n",dst_n_regions,dst_drive);
	fprintf (log,"Destination disk layout: ");
	fprintf (log," %05llu/%03llu/%02llu ",
		dst_dcb->disk_max.cylinder,
		dst_dcb->disk_max.head,
		dst_dcb->disk_max.sector);
	fprintf (log,"%llu total sectors on disk\n",dst_dcb->n_sectors);
	fprintf (log,"%4s%10s%10s%10s%23s\n","","Start LBA","End LBA","Length",
		"Size: MB   (binary)");
	for (i = 0; i < dst_n_regions; i++){
		printf ("%2d %c %9llu %9llu %9llu %8.2fMB %8.2fBMB\n",
			i,
			dst_layout[i].chunk_class,
			dst_layout[i].lba_start,
			dst_layout[i].lba_start + dst_layout[i].n_sectors - 1,
			dst_layout[i].n_sectors,
			BYTES_PER_SECTOR * (double)dst_layout[i].n_sectors/1000000.0,
			BYTES_PER_SECTOR * (double)dst_layout[i].n_sectors/1048576.0);
		fprintf (log,"%2d %c %9llu %9llu %9llu %8.2fMB %8.2fBMB\n",
			i,
			dst_layout[i].chunk_class,
			dst_layout[i].lba_start,
			dst_layout[i].lba_start + dst_layout[i].n_sectors - 1,
			dst_layout[i].n_sectors,
			BYTES_PER_SECTOR * (double)dst_layout[i].n_sectors/1000000.0,
			BYTES_PER_SECTOR * (double)dst_layout[i].n_sectors/1048576.0);
	}
/******************************************************************************
Do the compare
******************************************************************************/
	if (layout_only == 0)
		status = do_compare(src_dcb,src_n_regions,src_layout,dst_dcb,dst_n_regions,
					dst_layout,src_fill,dst_fill,log,assign_regions,from);
/******************************************************************************
Close the log file
******************************************************************************/
	log_close(log,from);
	return 0;
}
