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
/***** Revised by: Ben Livelsberger, NIST/SDCT  ****/
/* Modified by Kelsey Rider, NIST/SDCT June 2004 */
# include <features.h>
# include <unistd.h>

# include <stdio.h>
# include "zbios.h"
# include <string.h>
# include <malloc.h>
# include <time.h>

# include <stdlib.h>
# include <linux/hdreg.h>
# include <linux/fs.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <errno.h>
# include <getopt.h>
# include <sys/ioctl.h>
# include <scsi/scsi.h>
# include <scsi/scsi_ioctl.h>

char *SCCS_Z = "@(#) zbios.c Linux Version 1.5 Created 03/21/05 at 09:09:12 "\
"\nsupport lib compiled "__DATE__" at "__TIME__"\n"Z_H_ID;
# define GET_DISK_PARMS 8


/*****************************************************************
Support Library
	Probe an IDE or SCSI disk for Mfg & Serial #
Log file: open, log disk, close
	Give user progress and completion time feedback
	Disk utilities: open, read, write, convert LBA <=> C/H/S
	Partition table: get and print
*****************************************************************/

/*****************************************************************
Handy function to commit any writing to disk and print errors
*****************************************************************/
int mysync(int file) {
	int err;

	if(err = fsync(file)) {
		printf("Unable to commit (sync) data to disk: %s\n", strerror(errno));
	}

	return err;
}

/*****************************************************************
Remove any leading or trailing blanks from s
*****************************************************************/
void trim(char *s /* s is the string to trim */)
{
	int	at = strlen(s), k;

/*****************************************************************
Scan s from the end toward the beginning to find last non-blank
*****************************************************************/
	while (at){
		at--;
		if (s[at] != ' '){
			s[at+1] = '\0'; /* snip trailing blanks */
			break;
		}
	}
	if (s[0] != ' ')return; /* done if no leading blanks */
	at = 0;
	while (s[at] == ' ') at++; /* find first non-blank */
	k = 0;
	while (s[at]){ /* move string on top of leading blanks */
		s[k++] = s[at++];
	}
	s[k] = '\0'; /* terminate string */
	return;
}

/*****************************************************************
Probe the drive for its serial and model numbers, number of 
sectors, and maximum chs address.
*****************************************************************/
int probe_serial_model (disk_control_ptr d)
{
	int		adapter_no = 0,
			unit_no = 0,
			q = 0,
			i,
			fd,
			code;
	unsigned long	sectors;
	double		mb,
			bmb;
	unsigned char	*cmd,
			*pagestart,
			buffer[64 * 1024 + 100];
	struct hd_geometry g;
	struct hd_driveid  h;

	/* get drive's file descriptor */
	d->fd = open(d->dev, O_RDWR);
	if (d->fd < 0) {
		printf("Unable to open %s ", d->dev);
		return 1;
	}

	/* set the number of cylinders, heads, and sectors */
	code = ioctl (d->fd,HDIO_GETGEO,&g);
	if (code){
		printf("ioctl(HDIO_GETGEO) status\t= %d\n", code);
		d->geometry_is_real = 0;
		return 1;
	}
	d->geometry_is_real = 1;  /* able to find the drive's chs address */

	n_cylinders(d) = (off_t) g.cylinders;
	n_heads(d) = (off_t) g.heads;
	d->disk_max.sector = (off_t) g.sectors;

	/* get number of sectors */
	code = ioctl (d->fd,BLKGETSIZE,&sectors);

	if (code) {
		printf("ioctl(BLKGETSIZE) status\t= %d\n", code);
		return 1;
	}
	d->n_sectors = (off_t) sectors;

	if(d->drive_type == DRIVE_IS_SCSI){

		/* get serial number */
		memset(buffer, 0, 10*1024);

		*((int *) buffer) = 0;	/* length of input data */
		*(((int *) buffer) + 1) = 1024;	/* length of output buffer */

		cmd = (char *) (((int *) buffer) + 2);

		cmd[0] = 0x12;		/* INQUIRY */
		cmd[1] = 0x01;		/* lun=0, evpd=1 */
		cmd[2] = 0x80;		/* page code = 0x80, serial number */
		cmd[3] = 0x00;		/* (reserved) */
		cmd[4] = 0xff;		/* allocation length */
		cmd[5] = 0x00;		/* control */

		code = ioctl(d->fd,/*1*/ SCSI_IOCTL_SEND_COMMAND, buffer);
		if (code){
			printf("Could not get serial number (EVPD not supported) (ioctl status %d)\n", code);
			/*return 1;*/
		} else {
			/* Set drive's serial number */
			pagestart = buffer + 8;
			for (i = 0; i < pagestart[3]; i++){
				d->serial_no[i] = pagestart[4 + i];
			}
			d->serial_no[i + 1] = '\0';
		}
 
		/* get drive model */
		memset(buffer, 0, 10*1024);

		*((int *) buffer) = 0;	/* length of input data */
		*(((int *) buffer) + 1) = 1024;	/* length of output buffer */

		cmd = (char *) (((int *) buffer) + 2);

		cmd[0] = 0x12;		/* INQUIRY */
		cmd[1] = 0x00;		/* lun=0, evpd=0 */
		cmd[2] = 0x00;		/* page code = 0 */
		cmd[3] = 0x00;		/* (reserved) */
		cmd[4] = 0xff;		/* allocation length */
		cmd[5] = 0x00;		/* control */

		code = ioctl(d->fd,/*1*/ SCSI_IOCTL_SEND_COMMAND, buffer);
		if (code){
			printf("Could not get drive model (ioctl status %d)\n", code);
			return 1;
		}

		pagestart = buffer + 8;
		strncpy(d->model_no, pagestart + 16, 16);
		d->model_no[16] = '\0';

	} else { /* is an IDE hard drive */

		code = ioctl (d->fd,HDIO_GET_IDENTITY,&h);
		if (code) {
			printf("ioctl(HDIO_GET_IDENTITY) status\t= %d\n", code);
			return 1;
		}

		/* Set Serial number */
		strncpy(d->serial_no, h.serial_no, 20);
		d->serial_no[20] = '\0';

		/* Set model number */
		strncpy(d->model_no, h.model, 40);
		d->model_no[40] = '\0';
	}

	return 0;
}

/*****************************************************************
Create an empty list of ranges
*****************************************************************/
range_ptr create_range_list(void)
{
	range_ptr	p;
	/* allocate space for a list */
	if((p = (range_ptr) malloc (sizeof(range_list))) == NULL) {
		printf("Unable to allocate memory!\n");
		exit(1);
	}
	p->n = 0; /* list starts out empty */
	p->is_more = 0;
	return p;
}

/*****************************************************************
Add x to the list of ranges (r)
	(1) look for a range that can be expanded to include x
		 i.e., range of form a--b where b+1=x, a--b can then be
		 expanded to a--x
	(2) If an expandable range can't be found create a range of
		 the form x--x
*****************************************************************/
void add_to_range (range_ptr r, off_t x)
{
	int k = r->n - 1;
	if (r->n){ /* there is already at least one range */
		/* if x is one past edge of last range, expand range */
		if (r->r[k].to + 1 == x) r->r[k].to = x;
		else { /* either add new range of count total in category */
			k++;
			if (k >= N_RANGE){ /* too many ranges, just count */
				r->is_more++;
				return;
			}
			/* add new range to list: x--x */
			r->n++;
			r->r[k].from = r->r[k].to = x;
		}
	}
	else { /* first time: set number of ranges in range list to 1 */
		r->n = 1;
		/* set range: x--x */
		r->r[0].from = r->r[0].to = x;
	}
}

/************************************************************************
            log -- log file
            caption -- caption for the log file 
	    range_ptr r -- range list to print
************************************************************************/
void print_range_list(FILE *log, char *caption, range_ptr r)
{
	int	i, nc = 0; /* track line length */

	nc = fprintf (log,"%s ",caption);
	for (i = 0; i < r->n; i++){
		if(i)nc += fprintf(log,", "); /* add a comma if more */
		if (nc > 50){ /* time for a new line */
			nc = 0;
			fprintf (log,"\n");
		}
		if (r->r[i].from == r->r[i].to)
			/* range beginning and ending points are the same */
			nc += fprintf (log,"%llu",r->r[i].from);
		else nc += fprintf (log,"%llu-%llu",r->r[i].from,r->r[i].to);
	}
	if (r->is_more) fprintf (log,". . . + %ld more\n",r->is_more);
	else fprintf (log,"\n");
}

/*****************************************************************
Start a logfile. If the logfile can't be opened use stdout
	name -- name for the logfile
	access -- "w" create new file, "a" append to old file
	comment -- Comment entry for log file
	stats[0] -- SCCS entry for program
	stats[1] -- date program compiled
	stats[2] -- time program compiled
	np -- number of command line parameters
	p[0] -- program name
	p[1] -- test case name
	p[2] -- computer host hame
	p[3] -- initials of person running test
*****************************************************************/
FILE * log_open(char *name, char *access, char *comment, char **stats,
	int np, char **p)
{
	FILE	*log;
	int	i;

	/* If no comment string then prompt to type
	a comment for the log file. If there is a comment already (i.e., from
	the command line, then use the name for the log file */
	if (strlen(comment) == 0) {
		printf ("Type a descriptive comment and press ENTER\n");
		fgets(comment, 80, stdin);
	}
	log = fopen (name,access);
	if (log == NULL){ 
		log = stdout; /* use stdout if no floppy */
		printf("open of log file unsuccessful...using stdout instead\n");
	}
	fprintf (log,"%s %s\ncompiled on %s at %s using gcc Version %s\n",
		p[0],stats[0],stats[1],stats[2],__VERSION__);
	fprintf (log,"%s\n",SCCS_Z); /* log support lib and header file version */
	fprintf (log,"cmd:"); /* copy command line to log file */
	for (i = 0; i < np; i++) fprintf (log," %s",p[i]);
	fprintf (log,"\n");
	fprintf (log,"TEST %s HOST %s OPERATOR %s \nComment: %s\n",p[1],p[2],p[3],comment);
	return log;
}

/*****************************************************************
Compute elapsed time and close a log file
*****************************************************************/
void log_close (FILE *log, /* log file */
		time_t from /* time program started running */)
{
	time_t		till; /* time program finished (actually time log_close called) */
	unsigned long	et, /* elapsed time (program run time: seconds of wall clock) */
			tmin, /* total minutes running */
			min, /* minutes for elapsed time after whole hours deducted */
			sec, /* seconds of elapsed run time after hours & minutes */
			hours; /* hours of elapsed time */

	time(&till); /* get current time */
	et = till - from; /* elapsed time in seconds */
	tmin = et/60; /* elapsed time in minutes */
	sec = et%60; /* fraction of last minute in seconds */
	hours = tmin/60; /* hours elapsed time */
	min = tmin%60; /* fraction of last hour in whole minutes */
   
	fprintf (log,"run start %s",ctime(&from));
	fprintf (log,"run finish %s",ctime(&till));

	fprintf (log,"elapsed time %lu:%lu:%lu\n",hours,min,sec);
	fprintf (log,"Normal exit\n"); 

	printf ("elapsed time %lu:%lu:%lu\n",hours,min,sec);
	printf ("Normal exit\n");
}

/*****************************************************************
Record information about a disk to a log file
	Type of BIOS: Legacy or extended
	Disk geometry reported by Legacy BIOS: int 13/cmd 8 (max values)
	Disk geometry reported by XBIOS: int 13/cmd 48 (number of values)
	For IDE disks connected to a standard IDE controler:
		Disk Model number
		Disk Serial number
		for disks that support LBA: max number of user accessable sectors
		reported by the controler.
*****************************************************************/
void log_disk(	FILE *log, /* log file */
		char *caption, /* caption for log file */
		disk_control_ptr d) /* disk to log information about */
{
	printf ("%s Drive %s \n", caption, &d->dev);
	fprintf (log,"%s Drive %s\n",caption,d->dev);
	fprintf (log,"%05llu/%03llu/%02llu (max cyl/hd values)\n",
		d->disk_max.cylinder-1,
		d->disk_max.head-1,
		d->disk_max.sector);
	fprintf (log,"%05llu/%03llu/%02llu (number of cyl/hd)\n",
		d->disk_max.cylinder,
		d->disk_max.head,
		d->disk_max.sector);
	fprintf (log,"%llu total number of sectors\n",
		d->n_sectors);

	if (d->disk_max.sector != DISK_MAX_SECTORS)
		fprintf (log, "WARNING: disk is not supported; it has other than %u sectors per track.\n", DISK_MAX_SECTORS);

	if (d->drive_type != DRIVE_IS_IDE){ 
		fprintf (log,"Non-IDE disk\n");
	} else {
		fprintf (log,"IDE disk: ");
	}

	fprintf (log,"Model (%s) serial # (%s)\n",d->model_no,d->serial_no);
}

/*****************************************************************
Give progress and completion time feedback to a user
	Intent is to call this on each pass of a loop that goes
	from "from" upto "to".
	start -- time the loop entered
	from  -- starting value of loop index
	at    -- current value of loop index
	to    -- terminal value of loop
*****************************************************************/
void feedback (time_t start, off_t from, off_t at,
					off_t to)
{
	time_t		now;
	int		every_pc = 5,
			n_feed = 100/every_pc; /* give feedback every n_feed% */
	off_t		feed = (to - from)/n_feed;
	unsigned long	et,
			hr,
			min,
			tmin,
			sec; /* estimated elapsed time */
	float		pc, /* percent completed */
			fat,
			ns; /* number completed */
	static int	first = 1; /* first time called switch */

	at++;
	if (feed == 0) feed = 1;
	if (first) {/* first time through tell the user how often to
			expect feed back */
		first = 0;
		printf ("Feedback every %llu sectors (%d%%) of %llu\n",feed,every_pc,
			to-from);
	}

	if (((at - from)%feed) && (at != to)) return;
	if (from)
		printf ("at %llu (%llu) of %llu (%llu) from %llu",
			at,at-from,to,to-from,from);
	else printf ("at %llu of %llu",at,to);

	fat = at - from;
	ns = to - from;
	pc = (100.00*fat)/ns; /* pc = percent completed so far */
	time(&now);
	et = (now - start)*(ns - fat)/fat; /* et estimates time remaining */
	tmin = et/60;
	hr = tmin/60;
	min = tmin%60;
	sec = et%60;

	printf (" %5.1f%%    %lu:%02lu:%02lu remains on %s",pc,hr,min,
		sec,ctime(&now));
	return;
}

/*****************************************************************
Write a track (63 sectors) to disk d at address a
See the ATA BIOS extensions documents for details
*****************************************************************/
int disk_write (disk_control_ptr d, chs_addr *a)
{
	off_t	lba,
		lseek_err;
	ssize_t	write_err;

	/* convert C/H/S address to a Logical Block Address */
	lba = (a->cylinder*d->disk_max.head + a->head)*63 + a->sector - 1;

	/* position file pointer to correct position */
	lba *= BYTES_PER_SECTOR;
	lseek_err = lseek(d->fd, lba, SEEK_SET);

	if(lseek_err == (off_t) -1) {
		printf("Error: %i (%s) has occurred attempting to seek on %s (lba: %llu)\n", errno, strerror(errno), d->dev, lba/BYTES_PER_SECTOR);
		printf("   Cylinder %llu, Head %llu, Sector %llu\n", a->cylinder, a->head, a->sector);

		return lseek_err;
	}

	/* do the write to the hard drive */
	write_err = write(d->fd, d->buffer, DISK_MAX_SECTORS * BYTES_PER_SECTOR);

	if (!write_err) {
		/* end of file */
		printf("An attempt was made to access an invalid address(LBA): %llu on %s\n", lba/BYTES_PER_SECTOR, d->dev); 
		return 1;
	} else if (write_err < 0) {
		printf("Error: %i (%s) has occurred attempting to write to %s\n", errno, strerror(errno), d->dev);
		return write_err;
	}

	return 0;
}

/*****************************************************************
Read a track from disk d at address a
*****************************************************************/
int disk_read (disk_control_ptr d, chs_addr *a)
{
	off_t	lseek_err,
		lba;
	ssize_t	read_err;  

	/* convert C/H/S address to a Logical Block Address */
	lba = (a->cylinder*d->disk_max.head + a->head)*63 + a->sector - 1;

	/* position file pointer to correct position */
	lba *= BYTES_PER_SECTOR;
	lseek_err = lseek(d->fd, lba, SEEK_SET);

	if(lseek_err == -1) {
		printf("Error: %i (%s) has occurred attempting to seek on %s (lba: %llu)\n", errno, strerror(errno), d->dev, lba/BYTES_PER_SECTOR);
		printf("   Cylinder %llu, Head %llu, Sector %llu\n", a->cylinder, a->head, a->sector);
		return lseek_err;
	}

	/* do the read from the hard drive */
	read_err = read(d->fd, d->buffer, DISK_MAX_SECTORS * BYTES_PER_SECTOR /* 63 sectors X 512 bytes */);

	if (!read_err) {
		/* end of file */
		printf("an attempt was made to access an invalid address(LBA): %llu on %s\n", lba/BYTES_PER_SECTOR, d->dev); 
		return 1;
	} else if (read_err < 0) {
		printf("Error: %i (%s) has occurred attempting to read from %s\n", errno, strerror(errno), d->dev);
		return read_err;
	}

	return 0;
}

/*****************************************************************
Read a track from disk d at address lba
*****************************************************************/
int disk_read_lba (disk_control_ptr d, off_t lba)
{
	off_t	lseek_err;
	ssize_t	read_err;  

	/* position file pointer to correct position */
	lba *= BYTES_PER_SECTOR;
	lseek_err = lseek(d->fd, lba*BYTES_PER_SECTOR, SEEK_SET);

	if(lseek_err == -1) {
		printf("Error: %i (%s) has occurred attempting to seek on %s (lba: %llu)\n", errno, strerror(errno), d->dev, lba/BYTES_PER_SECTOR);
		return lseek_err;
	}

	/* do the read from the hard drive */
	read_err = read(d->fd, d->buffer, DISK_MAX_SECTORS * BYTES_PER_SECTOR /* 63 sectors X 512 bytes */);

	if (!read_err) {
		/* end of file */
		printf("an attempt was made to access an invalid address(LBA): %llu on %s\n", lba/BYTES_PER_SECTOR, d->dev); 
		return 1;
	} else if (read_err < 0) {
		printf("Error: %i (%s) has occurred attempting to read from %s\n", errno, strerror(errno), d->dev);
		return read_err;
	}

	return 0;
}

/*****************************************************************
Open a disk, return a pointer to a disk_control_rec
The disk_control_rec contains a description of the disk ...
	legacy BIOS geometry
	extended BIOS geometry
	IDE or not
	number of sectors addressable
	number of sectors reported by BIOS
	IDE disk model and serial numbers
*****************************************************************/
disk_control_ptr open_disk (char *drive, int *err)
{
	disk_control_ptr	d;

	d = (disk_control_ptr) malloc (sizeof(disk_control_block));
	if (d == NULL) { /* out of memory */
		*err = 3000;
		return NULL;
	}

	/* set drive name */
	strncpy(((char *) &d->dev), drive, NAME_LENGTH - 1);

	/* set drive type */
	if (drive[5] == 's')
		d->drive_type = DRIVE_IS_SCSI;
	else
		d->drive_type = DRIVE_IS_IDE;  

	/* Set model and serial numbers, number of sectors, and CHS maximum */
	*err = probe_serial_model(d);

	if (d->fd > 0) printf ("Open %s %s %llu on drive %s\n",
		d->model_no, d->serial_no, d->n_sectors, drive);
	else printf ("Open non-IDE (SCSI?) disk on drive %s\n",drive);

	return d;
}

/*****************************************************************
Convert LBA value to C/H/S
*****************************************************************/
void lba_to_chs (disk_control_block *d, off_t lba, chs_addr *a)
{
	off_t track = lba/63;
	a->sector = lba%63 + 1;
	a->head = track%n_heads(d);
	a->cylinder = track/n_heads(d);
}

/*****************************************************************
Read sector "lba" of disk d set b to point to start of sector
(An easier interface than C/H/S)
*****************************************************************/
int read_lba (disk_control_ptr d, off_t lba, unsigned char **b)
{
	chs_addr	a;

	*b = (unsigned char *)&(d->buffer[lba % DISK_MAX_SECTORS][0]);
	lba_to_chs (d,lba,&a);  /* convert lba to CHS */
	a.sector = 1; /* Fix sector count */
	return disk_read(d,&a); /* do the read */
}

/*****************************************************************
Get a nested partition table entry
*****************************************************************/

pte_ptr get_sub_part (disk_control_block *d,off_t start,
	off_t base,int *status)
{
	mbr_sector *mbr;
	pte_ptr	p,q;
	int 		i;
	off_t at = base + start;
	chs_addr a;

	lba_to_chs(d,at,&a);
	*status = disk_read (d,&a);
	mbr = (mbr_sector *)(&(d->buffer));
	if (*status) return NULL;
	if (mbr->sig == 0xAA55) {
		p = q = (pte_ptr) malloc (sizeof(pte_rec));
		for (i = 0; i < 2; i++){
			p->is_boot = mbr->pe[i].bootid;
			p->type = mbr->pe[i].type_code;
			p->lba_start = mbr->pe[i].starting_lba_sector;
			p->lba_length = mbr->pe[i].n_sectors;
			p->start.cylinder = mbr->pe[i].start_cylinder |
				 ((mbr->pe[i].start_sector&0xC0)<<2);
			p->start.head =  mbr->pe[i].start_head;
			p->start.sector = mbr->pe[i].start_sector & 0x3F;
			p->end.cylinder = mbr->pe[i].end_cylinder |
				 ((mbr->pe[i].end_sector&0xC0)<<2);
			p->end.head =  mbr->pe[i].end_head;
			p->end.sector = mbr->pe[i].end_sector & 0x3F;
			if (i == 0){
				p->next = (pte_ptr) malloc (sizeof(pte_rec));
				p = p->next;
			}
			else {
				if ((mbr->pe[1].type_code == 0x05) ||
					 (mbr->pe[1].type_code == 0x0F)
					 ){
					p->next = get_sub_part(d,p->lba_start,base,status);
				}
				else p->next = NULL;
			}
		}
	}
	else return NULL;
	return q;
}

/*****************************************************************
Get the partition table for disk d and save in pt
*****************************************************************/
int get_partition_table(disk_control_block *d,pte_ptr pt)
{
	mbr_sector *mbr;
	chs_addr  boot = {0ul,0ul,1ul};
	int	status,i;

	status = disk_read (d,&boot);
	mbr = (mbr_sector *) &(d->buffer);
	if (status) return status;
	if (mbr->sig != 0xAA55) return -1;
	else {
		for (i = 0; i < 4; i++){
			status = disk_read (d,&boot); /* really needed */
			pt[i].is_boot = mbr->pe[i].bootid;
			pt[i].type = mbr->pe[i].type_code;
			pt[i].lba_start = mbr->pe[i].starting_lba_sector;
			pt[i].lba_length = mbr->pe[i].n_sectors;
			pt[i].start.cylinder = mbr->pe[i].start_cylinder |
				 ((mbr->pe[i].start_sector&0xC0)<<2);
			pt[i].start.head =  mbr->pe[i].start_head;
			pt[i].start.sector = mbr->pe[i].start_sector & 0x3F;
			pt[i].end.cylinder = mbr->pe[i].end_cylinder |
				 ((mbr->pe[i].end_sector&0xC0)<<2);
			pt[i].end.head =  mbr->pe[i].end_head;
			pt[i].end.sector = mbr->pe[i].end_sector & 0x3F;
			if ((mbr->pe[i].type_code == 0x05) ||
				 (mbr->pe[i].type_code == 0x0F)
				 ){
					pt[i].next = get_sub_part (d,(off_t)0ul,pt[i].lba_start,&status);
					if (status) return status;
			}
			else {
				pt[i].next = NULL;
			}
		 }

	}
	return status;

}
/*****************************************************************
Map common partition type codes to an ASCII string
*****************************************************************/
char *partition_type(unsigned char code)
{
	char	*pt;

	if (code == 0) pt = "empty entry";
	else if (is_extended(code)) pt = "extended";
	else if ((code == 0x04) || (code == 0x06) || (code == 0x0E)) pt = "Fat16";
	else if ((code == 0x0B)) pt = "Fat32"; 
	else if ((code == 0x0C)) pt = "Fat32X";
	else if ((code == 0x01)) pt = "Fat12";
	else if ((code == 0x07)) pt = "NTFS";
	else if ((code == 0x82)) pt = "Linux swap";
	else if ((code == 0x81) || (code == 0x83)) pt = "Linux";
	else pt = "other";
	return pt;
}
 
/*****************************************************************
Print a partition table
	full=1 -- print extended table entries too
*****************************************************************/
void print_partition_table(FILE    *log,/* output file: either log file or stdout */
			   pte_rec *p, /* partition table */
			   int     index, /* print the index numbers */
			   int     full) /* print all table entries, else omit empty entries
					 and extended partition entries */
{
	int	i,
		j = 0;
	char	type_code;
	pte_ptr	sub;

	if (index) fprintf (log, " N ");
	fprintf (log,"  %-9s %-9s %-11s %-11s boot Partition type\n",
		"Start LBA","Length","Start C/H/S","End C/H/S");
	for (i = 0; i < 4; i++) {
		if (is_extended(p[i].type)) type_code = 'X';
		else type_code = 'P';
		j++;
		if (full || ((type_code != 'X') && (p[i].type))) {
			if (index) fprintf (log,"%2d ",j);
			fprintf (log,"%c %09llu %09llu %04llu/%03llu/%02llu %04llu/%03llu/%02llu %4s %02X",
				type_code,
				p[i].lba_start, p[i].lba_length,
				p[i].start.cylinder, p[i].start.head, p[i].start.sector,
				p[i].end.cylinder, p[i].end.head, p[i].end.sector,
				p[i].is_boot ? "Boot" : "", p[i].type);
			fprintf (log," %s",partition_type(p[i].type));
			fprintf(log,"\n");
		}
		sub = p[i].next;
		while(sub) {
			if (is_extended(sub->type)) type_code = 'x';
			else type_code = 'S';
			j++;
			if (full || ((type_code != 'x') && (sub->type))) {
				if (index) fprintf (log,"%2d ",j);
					fprintf (log,"%c %09llu %09llu %04llu/%03llu/%02llu %04llu/%03llu/%02llu %4s %02X",
						type_code,
						sub->lba_start,sub->lba_length,
						sub->start.cylinder,sub->start.head,sub->start.sector,
						sub->end.cylinder,sub->end.head,sub->end.sector,
						sub->is_boot?"Boot":"",sub->type);
					fprintf (log," %s",partition_type(sub->type));
					fprintf(log,"\n");
			}
			sub = sub->next;
		}
	}
	if (full) {
		fprintf (log,"P primary partition (1-4)\n"); 
		fprintf (log,"S secondary (sub) partition\n");
		fprintf (log,"X primary extended partition (1-4)\n");
		fprintf (log,"x secondary extended partition\n");
	}
}
