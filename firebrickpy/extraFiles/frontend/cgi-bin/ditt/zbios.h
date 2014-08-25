# define Z_H_ID "@(#) zbios.h Linux Version 1.1 Created 02/10/05 at 10:53:24"
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
/***** Revised by Ben Livelsberger, NIST/SDCT ****/
/* Modified by Kelsey Rider, NIST/SDCT June 2004 */
# include <time.h>
# include <sys/types.h>

#define DRIVE_IS_IDE 0
#define DRIVE_IS_SCSI 1

#define GET_DISK_PARMS 8
#define N_RANGE 20
#define PK __attribute__ ((packed))

#define NAME_LENGTH 80
#define DISK_MAX_SECTORS 63
#define BYTES_PER_SECTOR 512
#define BUFF_OFF 30
#define MAX_OFF_T 0xFFFFFFFFFFFFFFFFull
#define MAX_PARTITIONS 25

#define CHUNK_PARTITION 'P'
#define CHUNK_BOOT 'B'
#define CHUNK_BOOT_EXT 'b'
#define CHUNK_UNALLOCATED 'U'

#define n_sectors(d)	((d)->n_sectors)
#define n_cylinders(d)	((d)->disk_max.cylinder)
#define n_heads(d)	((d)->disk_max.head)
#define n_tracks(d)	((d)->disk_max.head*(d)->disk_max.cylinder)

#define is_extended(t)	((t == 0x05) || (t == 0x0F))

/******************************************************************************
A disk address in cylinder/head/sector format
******************************************************************************/
typedef struct {
	off_t	cylinder,
		head,
		sector;
} chs_addr; /* C/H/S disk address */

/******************************************************************************
The disk_control_block contains all information about a disk drive
Disk geometry as seen by legacy BIOS (interrupt 13/command 0x08): logical disk
Disk geometry as seen by XBIOS (int 13/cmd 0x48): disk_max
number of sectors reported by BIOS: n_sectors
A buffer holding 63 sectors: buffer
Drive number: drive
Flag indicating XBIOS active: use_bios_x
IDE Drive information: ide_info
******************************************************************************/

typedef unsigned char physical_sector[BYTES_PER_SECTOR]; /* a sector of 512 bytes */
typedef physical_sector physical_track[DISK_MAX_SECTORS]; /* a track is an array of 63 sectors */
typedef struct disk_struct disk_control_block, *disk_control_ptr;

struct disk_struct {
	char		dev[NAME_LENGTH];	/* drive name */
        char		serial_no[21];	/* Serial#  */
        char		model_no[41];	/* Model#   */
        int		fd;		/* reference to the hard drive*/
        int		drive_type;	/* SCSI or IDE */
	off_t		n_sectors;
	chs_addr	disk_max;	/* number of cyl, number of head */
        int		geometry_is_real;/* 1 if able to find C/H/S; 0 if unable */
        physical_track	buffer;		/* 63 sectors (1 track) buffer[sector][byte] */
};

/******************************************************************************
Partition table entry layout on disk
******************************************************************************/

typedef struct pts { /* partition table layout */
	unsigned char	bootid,
			start_head,
			start_sector,
			start_cylinder,
			type_code,
			end_head,
			end_sector,
			end_cylinder;
	unsigned int	starting_lba_sector,
			n_sectors;
} partition_table_rec, *partition_table_pointer;
/******************************************************************************
Layout of a partition table in a boot sector
******************************************************************************/

typedef struct { /* partition table in Master Boot Record */
	char			fill[446]; /* MBR boot code */
	partition_table_rec 	pe[4] PK; /* master partition table */
	unsigned short 		sig;  /* partition table signature word 0xAA55 */
}mbr_sector,*mbr_ptr;

/******************************************************************************
Data structure to keep partition table information
******************************************************************************/

typedef struct pte_struct pte_rec, *pte_ptr;
struct pte_struct {
	pte_ptr		next;
	chs_addr	start,
			end;
	off_t		lba_start,
			lba_length;
	unsigned char	is_boot,
			type;
};


/******************************************************************************
Data structure to track ranges of integers. The compare programs examine each
disk sector in LBA address sequence and assign each sector to a catagory, e.g.,
dst sector that differs from corresponding src sector, zero filled, src filled,
dst filled, etc. This data structure is used to track blocks of
disk sector LBA addresses for sectors that are classified in the same catagory.
******************************************************************************/

typedef struct {off_t from, to;} lba_range;
typedef struct { /* structure to keep a list of ranges */
	int		n;
	long		is_more; /* more than N_RANGE ranges present (i.e., some not recorded) */
	lba_range	r[N_RANGE];
	} range_list,*range_ptr;


/******************************************************************************
Function decls for zbios.c
******************************************************************************/

int                     read_lba (disk_control_ptr, off_t, unsigned char **);
int                     disk_write (disk_control_ptr, chs_addr *);
int                     disk_read (disk_control_ptr, chs_addr *);
disk_control_ptr        open_disk (char *, int *);
void 			lba_to_chs (disk_control_block *, off_t, chs_addr *);
FILE 			*log_open (char *, char *, char *, char **, int, char **);
void 			log_close (FILE *,time_t);
void 			log_disk(FILE *, char *, disk_control_ptr);
int 			get_partition_table(disk_control_block *,pte_ptr );
void 			print_partition_table(FILE *, pte_rec *, int, int);
void 			feedback (time_t, off_t, off_t, off_t);
range_ptr 	        create_range_list(void);
void 			add_to_range (range_ptr, off_t );
void 			print_range_list(FILE *, char *,range_ptr);

/* Helper functions */
void			print_rw_error(int);
int			mysync(int);
void			trim(char *);
