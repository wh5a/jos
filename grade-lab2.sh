#!/bin/sh

qemuopts="-hda obj/kern/kernel.img"
. ./grade-functions.sh


$make
run

score=0

echo_n "Page directory: "
 if grep "check_boot_pgdir() succeeded!" jos.out >/dev/null
 then
	score=`expr 20 + $score`
	echo OK $time
 else
	echo WRONG $time
 fi

echo_n "Page management: "
 if grep "page_check() succeeded!" jos.out >/dev/null
 then
	score=`expr 30 + $score`
	echo OK $time
 else
	echo WRONG $time
 fi

echo "Score: $score/50"

if [ $score -lt 50 ]; then
    exit 1
fi
