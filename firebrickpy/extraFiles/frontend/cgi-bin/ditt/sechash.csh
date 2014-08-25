#!/bin/csh -f
# The software provided here is released by the National
# Institute of Standards and Technology (NIST), an agency of
# the U.S. Department of Commerce, Gaithersburg MD 20899,
# USA.  The software bears no warranty, either expressed or
# implied. NIST does not assume legal liability nor
# responsibility for a User's use of the software or the
# results of such use.
# 
# Please note that within the United States, copyright
# protection, under Section 105 of the United States Code,
# Title 17, is not available for any work of the United
# States Government and/or for any works created by United
# States Government employees. User acknowledges that this
# software contains work which was created by NIST employees
# and is therefore in the public domain and not subject to
# copyright.  The User may use, distribute, or incorporate
# this software provided the User acknowledges this via an
# explicit acknowledgment of NIST-related contributions to
# the User's work. User also agrees to acknowledge, via an
# explicit acknowledgment, that any modifications or
# alterations have been made to this software before
# redistribution.
set need_help = 0
set too_few = 0
if ($#argv < 5) then
	set too_few = 1
else
	set TestCase = $1
	set host = $2
	set user = $3
	set device = $4
	set tail = $4:t
	set label = $5
	set cmd = ($argv)
	echo "Case $TestCase Host $host User $user Device $device Label $label"
	shift
	shift
	shift
	shift
	shift
endif
@ cnt = 0
set log = "none.txt"
set look_for = 0
set look_for_comment = 0
set look_for_first = 0
set look_for_last = 0
set need_comment = 1
set clear_log = 0
set look_for_hash = 0
set hash_prog = sha1sum
@ first_sector = 0
@ last_sector = -1
foreach arg ($argv:q)
#	echo "Remove this line arg: $arg"
	if ("$arg" == "-before") then
		@ cnt++
		set log = "hashbsec.txt"
	else if ("$arg" == "-after") then
		@ cnt++
		set log = "hashasec.txt"
	else if ("$arg" == "-new_log") then
		set clear_log = 1
	else if ("$arg" == "-log_name") then
		@ cnt++
		set look_for = 1
	else if ("$arg" == "-first") then
		set look_for_first = 1
	else if ("$arg" == "-last") then
		set look_for_last = 1
	else if ("$arg" == "-hash") then
		set look_for_hash = 1
	else if ("$arg" == "-comment") then
		set look_for_comment = 1
	else if ("$arg" == "-h") then
		set need_help = 1
		set too_few = 0
		@ cnt = 0
		break
	else if ("$look_for_first" == 1) then
		set look_for_first = 0
		@ first_sector = "$arg"
	else if ("$look_for_last" == 1) then
		set look_for_last = 0
		@ last_sector = "$arg"
	else if ("$look_for" == 1) then
		set look_for = 0
		set log = "$arg"
	else if ("$look_for_comment" == 1) then
		set comment = "$arg"
		set need_comment = 0
		set look_for_comment = 0
	else if ("$look_for_hash" == 1) then
		set hash_prog = "$arg"
		set look_for_hash = 0
	else
			echo "Invalid parameter $arg"
#		echo "The string ($arg) is out of place and not a valid parameter"
		set need_help = 1
	endif
end
	if ("$look_for" == 1) then
		echo "-log_name option requires a log filename"
#		echo "Log file name is missing after -log_name option"
		set need_help = 1
	endif
	if ("$look_for_comment" == 1) then
		echo "-comment option requires a comment"
#		echo "Comment is missing after -comment option"
		set need_help = 1
	endif
	if ("$look_for_hash" == 1) then
		echo "-hash option requires the name of a hash program"
		set need_help = 1
	endif
	if (($look_for_first == 1) || ($look_for_last == 1)) then
		echo "Sector number is missing after either -first or -last option"
		set need_help = 1
	endif
	if ("$cnt" == 0) then
#		echo "No log file specified. One of -before, -after or -log_name is required"
		echo "Must select -before, -after, or -log_name <name>"
		set need_help = 1
	endif
	if ("$cnt" > 1) then
		echo "Too many log files specified"
		set need_help = 1
	endif
	if ($too_few == 1) then
		echo "At least one required parameter is missing"
		set need_help = 1
	endif
	if ($need_help == 0) then
		set desc = `dmesg | grep sectors | grep $tail | tail -1`
		@ n_sectors = $desc[4]
		if ($last_sector == -1) @ last_sector = $n_sectors - 1
		if ($last_sector < $first_sector) then
			echo "Last sector ($last_sector) is before first sector ($first_sector)"
			set need_help = 1
		endif
		if ($first_sector < 0) then
			echo "First sector ($first_sector) is negative"
		set need_help = 1
		endif
		if ($last_sector >= $n_sectors) then
			echo "Last sector ($last_sector) is after end of drive ($n_sectors)"
		set need_help = 1
		endif
	endif
	if ($need_help == 1) then
	echo "usage: sechash.csh TestCase Host User Device Label [-options]"
		echo "Options:"
        echo "-before           Name the logfile hashblog.txt"
        echo "-after            Name the logfile hashalog.txt"
        echo "-first <LBA>      Start hashing at <LBA>"
        echo "-last <LBA>       Stop hashing at <LBA>"
        echo "-comment <text>   Record text in log"
        echo "-hash <prog_name> Use <prog_name> to compute a hash"
        echo "-new_log          Create a new log file"
        echo "-log_name <name>  Name the log file <name>"
        echo "-h                Print this list of options"
		exit 1
	endif
	if ($need_comment == 1) then
		echo "Please enter a descriptive comment"
		set comment = "$<"
	endif
	if ("$clear_log" == 1) then
		echo "@(#) sechash.csh Linux Version 1.8 Created 03/18/05 at 11:11:24" > $log
	else
		echo "@(#) sechash.csh Linux Version 1.8 Created 03/18/05 at 11:11:24" >> $log
	endif
	set start_time = `date`
	echo "CMD: $0 $cmd" >> $log
	echo "Case: $TestCase" >>$log
	echo "Host: $host" >>$log
	echo "User: $user" >>$log
	echo "Device: $device" >>$log
	echo "Label: $label" >>$log
	echo "Comment: $comment" >>$log
	echo "Hash: $hash_prog" >> $log
	uname -a >> $log
	$hash_prog --version | head -1 >> $log
	dmesg | grep sectors | grep $tail >> $log
	if ($tail =~ hd?) then
		/sbin/hdparm -i $device >> $log
	endif
	echo "run start $start_time"
	@ sector_count = 1 + $last_sector - $first_sector
	if ($tail =~ hd?) then
		/sbin/hdparm -i $device >> $log
	endif
	echo "Hash $sector_count sectors from $first_sector through $last_sector" >> $log
	echo "(dd bs=512 if=$device skip=$first_sector count=$sector_count | $hash_prog | tr a-z A-Z >> $log ) >>& $log" >> $log
	(dd bs=512 if=$device skip=$first_sector count=$sector_count | $hash_prog | tr a-z A-Z >> $log ) >>& $log
	set end_time = `date`
	tail -1 $log
	echo "run start $start_time" >> $log
	echo "run finish $end_time" >> $log
	echo " " >> $log
	echo " " >> $log
	echo "run finish $end_time"
