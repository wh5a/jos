#!/bin/sh

qemuopts="-hda obj/kern/kernel.img -hdb obj/fs/fs.img -net user -net nic,model=i82559er"
. ./grade-functions.sh
brkfn=cons_getc


$make
run

score=0

# 10 points - run-testpteshare
pts=10
runtest1 -tag 'PTE_SHARE [testpteshare]' testpteshare \
	'fork handles PTE_SHARE right' \
	'spawn handles PTE_SHARE right' \

# 10 points - run-testfdsharing
pts=10
runtest1 -tag 'fd sharing [testfdsharing]' testfdsharing \
	'read in parent succeeded' \
	'read in child succeeded' \
	'write to file data page succeeded'

# 20 points - run-icode
pts=20
runtest1 -tag 'updated file system switch [icode]' icode \
	'icode: read /motd' \
	'This is /motd, the message of the day.' \
	'icode: spawn /init' \
	'init: running' \
	'init: data seems okay' \
	'icode: exiting' \
	'init: bss seems okay' \
	"init: args: 'init' 'initarg1' 'initarg2'" \
	'init: running sh' \

# 20 points - run-testshell
pts=20
timeout=60
runtest1 -tag 'shell [testshell]' testshell \
	'shell ran correctly' \

# 10 points - run-primespipe
pts=10
timeout=120
echo 'The primespipe test has up to 2 minutes to complete.  Be patient.'
runtest1 -tag 'primespipe' primespipe \
	! 1 2 3 ! 4 5 ! 6 7 ! 8 ! 9 \
	! 10 11 ! 12 13 ! 14 ! 15 ! 16 17 ! 18 19 \
	! 20 ! 21 ! 22 23 ! 24 ! 25 ! 26 ! 27 ! 28 29 \
	! 30 31 ! 32 ! 33 ! 34 ! 35 ! 36 37 ! 38 ! 39 \
	541 1009 1097

echo "Score: $score/70"

if [ $score -lt 70 ]; then
    exit 1
fi
