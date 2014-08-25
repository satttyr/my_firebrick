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
static char *SCCS_ID[] = {"@(#) diskchg.c Linux Version 1.5 Created 03/15/05 at 17:24:32",
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

# include <sys/types.h>
# include <errno.h>

# define DIO_READ	0
# define DIO_WRITE	1
/*****************************************************************
Change a disk
DISKCHG can be used to do any of the following:
	change a single selected byte on a disk
	zero a sector
	fill a sector
	print the contents of a sector


program outline
	get command line
	if (write function) {
		read byte to change
		write back new value
		read back to confirm
		log results
	} else if (read function ){
		read sector
		log results
	} else if (zero function ){
		write zeros to sector
		log results 
	} else if (fill function ){
		write fill values to sector
		log results
	} else if (examine){
		interactive: ask for address, offset and length
		print content in HEX 
		log results
	} else print help

*****************************************************************/
/*****************************************************************
Disk Read/write routine: read (or write) the sector at address <a>
if <op> is DIO_READ, then read; if <op> is DIO_WRITE, then write (default)
put the data in (or get the data from) <buffer>
*****************************************************************/
int disk_io (disk_control_ptr d, chs_addr *a, int op, unsigned char *buffer)
{
	off_t	lba,
		lseek_err;
	ssize_t	rw_err;

	/* Convert C/H/S to LBA */
	lba = (a->cylinder*d->disk_max.head + a->head) * DISK_MAX_SECTORS + (a->sector - 1);

	/* Adjust to byte offset and seek */
	lba *= BYTES_PER_SECTOR;
	lseek_err = lseek(d->fd, lba, SEEK_SET);

	if(lseek_err == (off_t) -1) {
		printf("Error: %i (%s) has occurred attempting to seek on %s (byte offset: %llu)\n", errno, strerror(errno), d->dev, lba);
		printf("   Cylinder %llu, Head %llu, Sector %llu\n", a->cylinder, a->head, a->sector);
		return lseek_err;
	}

	/* Do the read or write */
	if (op == DIO_READ) {
		rw_err = read(d->fd, buffer, BYTES_PER_SECTOR);
	} else { 
		rw_err = write(d->fd, buffer, BYTES_PER_SECTOR);
	}

	/* Deal with errors */
	if(!rw_err) {
		printf("An attempt was made to access an invalid address (LBA): %llu on %s\n", lba/BYTES_PER_SECTOR, d->dev);
		return 1;
	} else if(rw_err < 0) {
		printf("Error: %i (%s) has occurred attempting to %s from %s\n", errno, strerror(errno), 
			((op == DIO_READ) ? "read" : "write"), d->dev);
		return rw_err;
	} else { /* Everything ok, so now sync up */
		rw_err = mysync(d->fd);
	}

	return 0;
}

/*****************************************************************
Decode a text string to both an LBA value and a Cylinder/Head/Sector value.
If the text string is one integer then it is assumbed to be an LBA address
If the text string is three integers separated by slashes then it is
assumed to be an address specified as C/H/S.
Otherwise is an error (return 1)
The disk address is converted to the other style so that both styles of
disk address are returned to the caller.
The parameter heads is used to in the conversion.
*****************************************************************/

int decode_disk_addr (char *addr, /* disk address as a string */
			unsigned long heads, /* number of heads per cylinder */
			off_t *lba, /* lba disk address */
			chs_addr *chs) /* C/H/S disk address */
{
	int		n; /* number of items decoded from addr: 1=>LBA, 3=>CHS */
	off_t		track;

	chs->head = 0;
	chs->sector = 0;
	*lba = 0;
	n = sscanf (addr,"%llu/%llu/%llu",&chs->cylinder,&chs->head,&chs->sector);
	if (n == 1) { /* one value ==> LBA address */
		 sscanf(addr, "%llu", lba);
		 chs->sector = (*lba)%63 + 1;
		 track = (*lba)/63;
		 chs->head = track%heads;
		 chs->cylinder = track/heads;
	} else if (n == 3){ /* three values separated by slashes ==> C/H/S */
		*lba = (chs->cylinder*heads + chs->head)*63 + chs->sector - 1;
	} else return 1; /* failed to get a valid disk address */
	return 0; /* OK */
}

/*****************************************************************
Replace the byte at <offset> in the sector at <addr> with <new_char>
*****************************************************************/
void update_sector (FILE *log, disk_control_ptr disk, chs_addr *addr, int offset,
							unsigned char new_char)
{
	unsigned char	sector[BYTES_PER_SECTOR],
			old_char;
	int		status;

	status = disk_io (disk, addr, DIO_READ, sector);
	if (status) {
		printf ("read for update failed\n"); 
		fprintf (log,"read for update failed\n");
		return;
	}
	old_char = sector[offset];
	sector[offset] = new_char;
	status = disk_io (disk, addr, DIO_WRITE, sector);
	if (status) {
		printf ("write for update failed\n");
		fprintf (log,"write for update failed\n");
		return;
	} else {
		fprintf (log,"Update sector, old value 0x%02X, new value 0x%02X\n",
			old_char,new_char);
	}
}

/*****************************************************************
Replace the sector at <addr_string> with zero bytes
*****************************************************************/
void zero_sector (FILE *log, disk_control_ptr d, char *addr_string)
{
	off_t		lba;
	chs_addr	chs;
	unsigned char	sector[BYTES_PER_SECTOR];
	int		i, status;

	status = decode_disk_addr (addr_string, (unsigned long) n_heads(d), &lba, &chs);
	if (status) {
		fprintf (log,"%s is not a valid sector address\n",addr_string);
		return; /* bad address, don't write */
	}
	for (i = 0; i < BYTES_PER_SECTOR; i++) sector[i] = '\0';
	status = disk_io (d, &chs, DIO_WRITE, sector);
	if (status) {
		fprintf (log,"Zero %s failed\n",addr_string);
	} else {
		fprintf (log,"\nDisk addr lba %llu  C/H/S %llu/%llu/%llu\n",
			lba,chs.cylinder,chs.head,chs.sector);
		fprintf (log,"Zero sector %s OK\n",addr_string);
	}
	return;
}


/*****************************************************************
Replace the sector at <addr_string> with a DISKWIPE style filled
sector. The fill values are for a sector at <fill_addr> of a disk
with <fill_heads> heads (disk geometry) and a fill byte of <fill_char>
*****************************************************************/
void fill_sector (FILE *log, disk_control_ptr d, char *addr_string,
	char *fill_addr, int fill_heads, unsigned char fill_char)
{
	off_t		lba, fill_lba;
	chs_addr	chs, fill_chs;
	unsigned char	sector[BYTES_PER_SECTOR];
	int		i, status;

	status = decode_disk_addr (addr_string,(unsigned long)n_heads(d),&lba,&chs);
	if (status) {
		fprintf (log,"%s is not a valid sector address\n",addr_string);
		return; /* bad address; don't write */
	}
	status = decode_disk_addr (fill_addr,(unsigned long)fill_heads,&fill_lba,&fill_chs);
	if (status) {
		fprintf (log,"%s (fill address) is not a valid sector address\n",addr_string);
		return; /* another bad address */
	}
	for (i = 0; i < BYTES_PER_SECTOR; i++) sector[i] = fill_char;
	sprintf ((char *)sector,"%05llu/%03llu/%02llu %012llu",
		fill_chs.cylinder,fill_chs.head,fill_chs.sector,fill_lba);
	status = disk_io (d, &chs, DIO_WRITE, sector);
	if (status) {
		fprintf (log,"Fill %s failed\n",addr_string);
	} else {
		fprintf (log,"\nDisk addr lba %llu  C/H/S %llu/%llu/%llu\n",
			lba,chs.cylinder,chs.head,chs.sector);
		fprintf (log,"\nUsing %d heads\n",fill_heads);
		fprintf (log,"Fill addr lba %llu  C/H/S %llu/%llu/%llu\n",
			fill_lba,fill_chs.cylinder,fill_chs.head,fill_chs.sector);
		fprintf (log,"Fill sector %s OK\n",addr_string);
	}
}


/*****************************************************************
Print <len> bytes starting at <offset> in the sector at <addr> on <disk>
*****************************************************************/
void print_sector (FILE *log, disk_control_ptr disk, chs_addr *addr, int offset, int len)
{
	unsigned char	sector[BYTES_PER_SECTOR];
	int		nc,
			i,
			status;

	status = disk_io (disk, addr, DIO_READ, sector);
	if (status) { /* read error */
		printf ("Disk read error 0x%02X at sector %llu/%llu/%llu\n",status,
			addr->cylinder,addr->head,addr->sector);
		fprintf (log,"Disk read error 0x%02X at sector %llu/%llu/%llu\n",status,
			addr->cylinder,addr->head,addr->sector);
		return;
	}
      
/*****************************************************************
Print the sector
*****************************************************************/
	nc = 0; /* nothing printed yet */
	for (i = offset; i < offset + len; i++){ /* loop over characters */
		if (nc == 0) { /* if start of line, print offset of first byte */
			printf ("%03d: ",i);
			fprintf (log,"%03d: ",i);
		}
		nc += printf (" %02X",sector[i]); /* print current byte */
		fprintf (log," %02X",sector[i]);
		if (nc > 45){  /* end of line? */
			printf ("\n");
			fprintf (log,"\n");
			nc = 0; /* new line, no chars printed yet */
		}
	}
	if (nc > 0){ /* anything on the line ? */
		 printf ("\n");
		 fprintf (log,"\n");
	}
}

/*****************************************************************
Print instructions about running cmd and options
*****************************************************************/
void print_help(char *p)
{
	static int been_here = 0;
	if (been_here) return;
	been_here = 1;

	printf ("Usage: %s test-case host operator drive [-options]\n",p);
	printf ("-comment \" ... \"\tGive comment on command line\n");
	printf ("-exam \tPrompt for sectors to print\n");
	printf ("-read addr offset length\n\tPrint <length> bytes starting at\n");
	printf ("\t<offset> from sector at <addr>\n");
	printf ("-write addr offset new_value\n\tReplace byte at\n");
	printf ("\t<offset> in sector at <addr> with <new_value> (in hex)\n");
	printf ("-fill addr fill_addr heads new_value\n\tFill sector at <addr>\n");
	printf ("\tin DISKWIPE style for address <fill_addr> using a disk geometry\n");
	printf ("\tof <heads> heads with fill byte of <new_value> (in hex)\n");
	printf ("\tif <heads> is zero, then number of heads on disk is used\n");
	printf ("-zero addr\n\tSet all bytes of sector at <addr> to zero\n");
	printf ("\t<addr> can be specified as either an LBA address (an integer)\n");
	printf ("\tor as cylinder/head/sector (three slash separated integers)\n");
	printf ("-new_log\tStart a new log file (default is append to old log file)\n");
	printf ("-log_name <name>\tUse different log file (default is chglog.txt)\n");
	printf ("-h\tPrint this option list\n");
}

main (int np, char **p) {
	int		help = 0,
			i; /* loop index */
	int		status;
	char		drive[NAME_LENGTH] = "/dev/hda"; 
	static time_t	from; /* start time */
	FILE		*log; /* the log file */
	static char	comment[NAME_LENGTH] = "";
	off_t		lba; /* file offset (location) to make change */
	int		offset = 0, /* offset within sector */
			len, /* number of bytes to print */
			new_char_in, /* input buffer for new fill char */
			fake_heads; /* number of heads for fill addr */
	unsigned char	new_char = 'Z'; /* the replacement character */

	 /* switches for options */
	int		is_read = 0,
			is_write = 0,
			is_exam = 0,
			is_zero = 0,
			is_fill = 0,
			lname_given = 0;
	char		*addr_string = "0", /* the disk address from command line */
			*fill_addr_string, /* disk address to fill with */
			disk_addr[NAME_LENGTH], /* disk addr from stdin */
			format[NAME_LENGTH] = "cg-%s-xlog.txt",
			log_name[NAME_LENGTH] = "chglog.txt",
			access[2] = "a";
	chs_addr	addr;
	disk_control_ptr disk;

	time(&from);
	printf ("\n%s compiled at %s on %s\n", p[0],
		__TIME__,__DATE__);

/*****************************************************************
Decode the command line
*****************************************************************/
		/* get the command line */
	if (np < 6) help = 1;
	for (i = 5; i < np; i++) {
		if (strcmp (p[i],"-h") == 0) {help = 1; break;}
		else if (strcmp (p[i],"-new_log")== 0) access[0] = 'w';
		else if (strcmp (p[i], "-log_name") == 0) {
			if(++i >= np) {
				printf("%s: -log_name option requires a logfile name\n", p[0]);
				help = 1;
			} else {
				strncpy(log_name, p[i], NAME_LENGTH);
				lname_given = 1;
			}
/*****************************************************************
Decode -read option
*****************************************************************/
		} else if (strcmp (p[i],"-read") == 0) {
			is_read = 1;
			i = i + 3;
			if (i >= np) {
				printf ("%s: -read must be followed by disk_addr offset length\n", p[0]);
				help = 1; break;
			} else {
				addr_string = p[i-2];
				sscanf (p[i-1],"%d",&offset);
				sscanf (p[i],"%d",&len);
				strcpy (format,"cg-%s-rlog.txt");
			}
/*****************************************************************
Decode -write option
*****************************************************************/
		} else if (strcmp (p[i],"-write") == 0) {
			is_write = 1;
			i = i + 3;
			if (i >= np) {
				printf ("%s: -write must be followed by disk_addr offset value\n", p[0]);
				help = 1; break;
			} else {
				addr_string = p[i-2];
				sscanf (p[i-1],"%d",&offset);
				sscanf (p[i],"%x",&new_char_in);
				new_char = (unsigned char) new_char_in;
				strcpy (format,"cg-%s-wlog.txt");
			}
		}
/*****************************************************************
Decode -fill option
*****************************************************************/
		else if (strcmp (p[i],"-fill") == 0) {
			is_fill = 1;
			i = i + 4;
			if (i >= np) {
				printf ("%s: -fill must be followed by disk_addr fill_addr heads value\n", p[0]);
				help = 1; break;
			} else {
				addr_string = p[i-3];
				fill_addr_string = p[i-2];
				sscanf (p[i-1],"%d",&fake_heads);
				sscanf (p[i],"%x",&new_char_in);
				new_char = (unsigned char) new_char_in;
				strcpy (format,"cg-%s-flog.txt");
			}
		}
/*****************************************************************
Decode -zero option
*****************************************************************/
		else if (strcmp (p[i],"-zero") == 0) {
			is_zero = 1;
			i = i + 1;
			if (i >= np) {
				printf ("%s: -zero must be followed by disk_addr\n", p[0]);
				help = 1; break;
			} else {
				addr_string = p[i];
				strcpy (format,"cg-%s-zlog.txt");
			}
		}
/*****************************************************************
Decode -exam option
*****************************************************************/
		else if (strcmp (p[i],"-exam") == 0) {
			is_exam = 1;
				strcpy (format,"cg-%s-xlog.txt");
		} else if (strcmp (p[i],"-comment")== 0) {
			i++;
			if (i >= np){
				printf ("%s: -comment option requires a comment\n", p[0]);
				help = 1;
			} else strncpy (comment,p[i], NAME_LENGTH - 1);
		} else {
			printf("Invalid parameter: %s\n", p[i]);
			help = 1;
		}
	}

/*****************************************************************
Only allow one option at a time
*****************************************************************/
	if ((is_read+is_write+is_exam+is_fill+is_zero) != 1) {
		printf ("%s: select exactly one of: -read, -write, -zero, -fill or -exam\n",p[0]);
		help = 1;
	}

	if(help) {
		print_help(p[0]);
		return 0;
	}

	strncpy(drive, p[4], NAME_LENGTH - 1);
 
/*****************************************************************
Check and log the disk drive
*****************************************************************/
	disk = open_disk (drive,&status);

	if (status) {
		printf ("%s could not access drive %s status code %d\n",
			p[0],drive,status);
	}
	if(lname_given == 0) sprintf (log_name,format,&drive[5]);
	log = log_open(log_name,access,comment,SCCS_ID,np,p);
	log_disk (log,"Target disk",disk);
	fprintf (log,"\n");
/*****************************************************************
Validate offset
*****************************************************************/
	if (is_read) {
		if ((offset < 0) || (offset >= BYTES_PER_SECTOR)) {
			fprintf (log,"Offset %d not valid ([0..%u]), reset to 0\n",offset, BYTES_PER_SECTOR - 1);
			offset = 0;
		}
	}
	if (is_write) {
		if ((offset < 0) || (offset >= BYTES_PER_SECTOR)) {
			fprintf (log,"Offset %d not valid ([0..%u])\n", offset, BYTES_PER_SECTOR - 1);
			printf ("Offset %d not valid ([0..%u])\n", offset, BYTES_PER_SECTOR - 1);
			return 1;
		}
	}
/*****************************************************************
Read and print a portion of a sector
*****************************************************************/
	if (is_read) {
		if ((len <= 0) || (len > BYTES_PER_SECTOR)) {
				fprintf (log,"Length (%d) not valid ([1..%u]); resetting to 16\n",
					BYTES_PER_SECTOR, len);
				len = 16;
		}
		if ((offset+len) > BYTES_PER_SECTOR) {
			fprintf (log,
				"Length (%d) goes past end of sector (%d); resetting to end of sector\n",
				len,len+offset);
			len = BYTES_PER_SECTOR - offset;
		}
		status = decode_disk_addr (addr_string, /* disk address as a string */
					(unsigned long)n_heads(disk), /* number of heads per cylinder */
					&lba, /* lba disk address */
					&addr); /* C/H/S disk address */
		if (status) {
			fprintf (log,"%s is not a valid sector address\n",addr_string);
			return 1;
		}
		if (addr.head >= n_heads(disk)) {
			fprintf (log,"WARNING: specified heads %llu larger than number of heads %llu\n",
				addr.head,n_heads(disk));
		}
		fprintf (log,"Disk addr lba %llu  C/H/S %llu/%llu/%llu offset %d\n",
			lba,addr.cylinder,addr.head,addr.sector,offset);
		print_sector (log,disk,&addr,offset,len);
	}
/*****************************************************************
Update a byte in a sector
*****************************************************************/
	else if (is_write) {
		status = decode_disk_addr (addr_string, /* disk address as a string */
					(unsigned long)n_heads(disk), /* number of heads per cylinder */
					&lba, /* lba disk address */
					&addr); /* C/H/S disk address */
		if (status) {
			fprintf (log,"%s is not a valid sector address\n",addr_string);
			return 1;
		} else {
			if (addr.head >= n_heads(disk)) {
				fprintf (log,"WARNING: specified heads %llu larger than number of heads %llu\n",
					addr.head,n_heads(disk));
			}
			fprintf (log,"Disk addr lba %llu  C/H/S %llu/%llu/%llu offset %d\n",
				lba,addr.cylinder,addr.head,addr.sector,offset);
			update_sector (log,disk,&addr,offset,new_char);
		}
	} 
/*****************************************************************
Zero a sector
*****************************************************************/
	else if (is_zero) {
		zero_sector (log,disk,addr_string);
	}
   
/*****************************************************************
Fill a sector
*****************************************************************/
	else if (is_fill) {
		fill_sector (log,disk,addr_string,fill_addr_string,fake_heads?fake_heads:
			(int)n_heads(disk),new_char);
	}
   
/*****************************************************************
Interactive: print from user specified sectors
*****************************************************************/
	else if (is_exam) {
		printf ("Enter disk_addr (LBA or C/H/S) offset length (CTRL-D for EOF): ");
		while (EOF != scanf ("%s %d %d", disk_addr, &offset, &len)) {
			printf ("Offset %d length %d\n", offset, len);
			fprintf (log,"\nOffset %d length %d\n", offset, len);
			status = decode_disk_addr (disk_addr,(unsigned long)n_heads(disk), &lba, &addr);
			if (status) {
				fprintf (log,"%s is not a valid sector address\n", addr_string);
				continue;
			}
			if ((len <= 0) || (len > BYTES_PER_SECTOR)) {
				fprintf (log,"Length (%d) is not valid ([1..%u]), resetting to 16\n",
					BYTES_PER_SECTOR, len); 
				printf ("Length (%d) is not valid ([1..%u]), resetting to 16\n",
					BYTES_PER_SECTOR, len);
				len = 16;
			}
			if ((offset < 0) || (offset >= BYTES_PER_SECTOR)) {
				fprintf (log,"Offset %d not valid ([0..%u]), reset to 0\n", offset, BYTES_PER_SECTOR - 1);
				printf ("Offset %d not valid ([0..%u]), reset to 0\n", offset, BYTES_PER_SECTOR - 1);
				offset = 0;
			}
			if ((offset+len) > BYTES_PER_SECTOR) {
				fprintf (log,"Length goes past end of sector; resetting to end of sector\n"); 
				printf ("Length goes past end of sector; resetting to end of sector\n");
				len = BYTES_PER_SECTOR - offset;
			}
			if (addr.head >= n_heads(disk)) {
				fprintf (log,"WARNING: specified heads %llu larger than number of heads %llu\n",
					addr.head,n_heads(disk));
				printf ("WARNING: specified heads %llu larger than number of heads %llu\n",
					addr.head,n_heads(disk));
			}
			fprintf (log,"Disk addr lba %llu  C/H/S %llu/%llu/%llu offset %d\n",
				lba,addr.cylinder,addr.head,addr.sector,offset);
			printf ("Disk addr lba %llu  C/H/S %llu/%llu/%llu offset %d\n",
				lba,addr.cylinder,addr.head,addr.sector,offset);
			print_sector (log,disk,&addr,offset,len);
			printf ("\nEnter disk_addr (LBA or C/H/S) offset length (CTRL-D for EOF): ");
		}
	}
	 /* ran OK, log results */
	log_close (log,from);
	return 0;
}
