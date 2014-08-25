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
set need_comment = 1
set clear_log = 0
set look_for_hash = 0
set hash_prog = sha1sum
foreach arg ($argv:q)
#	echo "Remove this line arg: $arg"
	if ("$arg" == "-before") then
		@ cnt++
		set log = "hashblog.txt"
	else if ("$arg" == "-after") then
		@ cnt++
		set log = "hashalog.txt"
	else if ("$arg" == "-new_log") then
		set clear_log = 1
	else if ("$arg" == "-log_name") then
		@ cnt++
		set look_for = 1
	else if ("$arg" == "-hash") then
		set look_for_hash = 1
	else if ("$arg" == "-comment") then
		set look_for_comment = 1
	else if ("$arg" == "-h") then
		set need_help = 1
		set too_few = 0
		@ cnt = 0
		break
	else if ("$look_for" == 1) then
		set look_for = 0
		set log = "$arg"
	else if ("$look_for_hash" == 1) then
		set hash_prog = "$arg"
		set look_for_hash = 0
	else if ("$look_for_comment" == 1) then
		set comment = "$arg"
		set need_comment = 0
		set look_for_comment = 0
	else
#		echo "The string ($arg) is out of place and not a valid parameter"
			echo "Invalid parameter $arg"
		set need_help = 1
	endif
end
	if ($too_few == 1) then
		echo "At least one required parameter is missing"
		set need_help = 1
	endif
	if ("$look_for" == 1) then
#		echo "Log file name is missing after -log_name option"
	echo "-log_name option requires a log filename"
		set need_help = 1
	endif
	if ("$look_for_hash" == 1) then
		echo "-hash option requires the name of a hash program"
		set need_help = 1
	endif
	if ("$look_for_comment" == 1) then
#		echo "Comment is missing after -comment option"
		echo "-comment option requires a comment"
		set need_help = 1
	endif
	if ("$cnt" == 0) then
		echo "Must select -before, -after, or -log_name <name>"
#		echo "No log file specified. One of -before, -after or -log_name is required"
		set need_help = 1
	endif
	if ("$cnt" > 1) then
		echo "Too many log files specified"
		set need_help = 1
	endif
	if ($need_help == 1) then
	echo "usage: diskhash.csh TestCase Host User Device Label [-options]"
		echo "Options:"
        echo "-before           Name the logfile hashblog.txt"
        echo "-after            Name the logfile hashalog.txt"
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
		echo "@(#) diskhash.csh Linux Version 1.7 Created 03/18/05 at 11:11:24" > $log
	else
		echo "@(#) diskhash.csh Linux Version 1.7 Created 03/18/05 at 11:11:24" >> $log
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
	dmesg | grep sectors | grep $tail | tail -1 >> $log
	if ($tail =~ hd?) then
		/sbin/hdparm -i $device >> $log
	endif
	echo "run start $start_time"
	echo "(dd bs=512 if=$device | $hash_prog | tr a-z A-Z >> $log ) >>& $log" >> $log
	(dd bs=512 if=$device | $hash_prog | tr a-z A-Z >> $log ) >>& $log
	set end_time = `date`
	tail -1 $log
	echo "run start $start_time" >> $log
	echo "run finish $end_time" >> $log
	echo " " >> $log
	echo " " >> $log
	echo "run finish $end_time"
