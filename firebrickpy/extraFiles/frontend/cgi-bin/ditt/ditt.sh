#!/bin/sh
#Disk Imaging Tool Testing
echo "Source Disk setup"
echo -n "source disk path:"
read "src"
echo -n "Host:"
read "host"
echo -n "Operator intials:"
read "op"
echo -n "Operating System:"
read "os"
./logsetup $src $host $op $os

#source disk initialisation
function sdisk{
./partab $case $host $op $src
echo "Warning source drive will be wiped proceed?[y/n]"
read "con"
if [ "$con" == "y*" ]
echo "Fill: enter a two digit hexvalue to fill the sectors"
read "sfill"
then ./diskwipe $case $host $op $src $sfill -src -noask   
else sha1sum $src > srcbhash.txt
fi
#diskhash and sechash
./diskhash.csh $case $host $op $src -after
./sechash.csh $case $host $op $src -after
./diskhash.csh $case $host $op $dst -before
./sechash.csh $case $host $op $dst -before
#partab
./partab $case $host $op $src -all
ddisk
}

#Log case details
echo "Case Details"
echo -n "Test Case:"
read "case"
echo -n "Destination Disk:"
read "dst"
echo -n "Media Disk:"
read "media"
./logcase $case $host $op $src $dst $media
#diskhash and sechash
./diskhash.csh $case $host $op $src -before 
./sechash.csh $case $host $op $src -before
sdisk
#Destination disk initialisation
function ddisk{
echo "enter a two digit hex value to fill the sectors:"
read "dfill"
./partab $case $host $op $dst -all
./diskwipe $case $host $op $dst $dfill -dst
#diskhash and sechash
./diskhash.csh $case $host $op $dst -after
./sechash.csh $case $host $op $dst -after
#choice for partition function
echo "Do you need partition?[y/n]"
read "part"
if [ "part" == "y*" ]
then partition
else media 
fi
}
#Partition function
function partition{
echo "Partition of the drive required"
if [ "$choice" == "y*" ] 

}
#Media Disk wipe
function media{
#diskhash and sechash
./diskhash.csh $case $host $op $media -before
./sechash.csh $case $host $op $media -before
echo "enter a unique pattern to the media disk"
read "mfill"
./partab $case $host $op $media -all
./diskwipe $case $host $op $media $mfill -media
#diskhash and sechash
./diskhash.csh $case $host $op $media -after
./sechash.csh $case $host $op $media -after
}
#use the tool to make an image file
#Corrupt
#restore an image file

#Measure 
function compare{
echo "Test case measurement choose beween:
diskcmp: compare two entire disks
partcmp: comapre two partitions
adjcmp: compare two entire disks when there has been cylinder alignment
seccmp: compare two sectors"
read "cmp"
if [ "$cmp" == "d*" ]
then ./diskcmp $case $host $op $src $sfill $dst $dfill
sha1sum $src > srcahash.txt
elif [ "$cmp" == "p*" ]
then ./partcmp $case $host $op $src $sfill $dst $dfill
sha1sum $src > srcahash.txt
elif [ "$cmp" == "a*" ]
then ./adjcmp $case $host $op $src $sfill $dst $dfill
sha1sum $src > srcahash.txt
elif [ "$cmp" == "s*" ]
then ./seccmp $case $host $op $src $sfill $dst $dfill 
else echo "skipping comparision"
exit  
fi
}
#corrupt
function corrupt{
echo -n "full path of the image-file:"
read "image"
echo -n "relative index position of the byte to be changed:"
read "index"
echo -n "hex value to replace selected bytes:"
read "val"
./corrupt $case $host $op $image $index $val  
}
#DISKCHG is intended as a tool to aid testing the other support programs. DISKCHG has
#the following functions:
#• Dump part of a sector in hexadecimal
#• Change a single byte of a sector
#• Zero the bytes of a sector
#• Fill a sector in DISKWIPE style
function diskchg{
echo -n "The drive device name. Example: /dev/sda"
read "drive"
./diskchg $case $host $op $drive 
}
 
