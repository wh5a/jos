#!/bin/sh

qemuopts="-hda obj/kern/kernel.img"
. ./grade-functions.sh


$make
run

score=0
total=0
timeout=10

runtest1 dumbfork \
	'.00000000. new env 00001000' \
	'.00000000. new env 00001001' \
	'0: I am the parent!' \
	'9: I am the parent!' \
	'0: I am the child!' \
	'9: I am the child!' \
	'19: I am the child!' \
	'.00001001. exiting gracefully' \
	'.00001001. free env 00001001' \
	'.00001002. exiting gracefully' \
	'.00001002. free env 00001002'

echo "Part A score: $score/5"
echo
total=`expr $total + $score`

score=0

runtest1 faultread \
	! 'I read ........ from location 0!' \
	'.00001001. user fault va 00000000 ip 008.....' \
        'TRAP frame at 0xf.......' \
	'  trap 0x0000000e Page Fault' \
	'  err  0x00000004' \
	'.00001001. free env 00001001'

runtest1 faultwrite \
	'.00001001. user fault va 00000000 ip 008.....' \
        'TRAP frame at 0xf.......' \
	'  trap 0x0000000e Page Fault' \
	'  err  0x00000006' \
	'.00001001. free env 00001001'

runtest1 faultdie \
	'i faulted at va deadbeef, err 6' \
	'.00001001. exiting gracefully' \
	'.00001001. free env 00001001' 

runtest1 faultalloc \
	'fault deadbeef' \
	'this string was faulted in at deadbeef' \
	'fault cafebffe' \
	'fault cafec000' \
	'this string was faulted in at cafebffe' \
	'.00001001. exiting gracefully' \
	'.00001001. free env 00001001'

runtest1 faultallocbad \
	'.00001001. user_mem_check assertion failure for va deadbeef' \
	'.00001001. free env 00001001' 

runtest1 faultnostack \
	'.00001001. user_mem_check assertion failure for va eebfff..' \
	'.00001001. free env 00001001'

runtest1 faultbadhandler \
	'.00001001. user_mem_check assertion failure for va (deadb|eebfe)...' \
	'.00001001. free env 00001001'

runtest1 faultevilhandler \
	'.00001001. user_mem_check assertion failure for va (f0100|eebfe)...' \
	'.00001001. free env 00001001'

runtest1 forktree \
	'....: I am .0.' \
	'....: I am .1.' \
	'....: I am .000.' \
	'....: I am .100.' \
	'....: I am .110.' \
	'....: I am .111.' \
	'....: I am .011.' \
	'....: I am .001.' \
	'.00001001. exiting gracefully' \
	'.00001002. exiting gracefully' \
	'.0000200.. exiting gracefully' \
	'.0000200.. free env 0000200.'

echo "Part B score: $score/45"
echo
total=`expr $total + $score`

score=0

runtest1 spin \
	'.00000000. new env 00001000' \
	'.00000000. new env 00001001' \
	'I am the parent.  Forking the child...' \
	'.00001001. new env 00001002' \
	'I am the parent.  Running the child...' \
	'I am the child.  Spinning...' \
	'I am the parent.  Killing the child...' \
	'.00001001. destroying 00001002' \
	'.00001001. free env 00001002' \
	'.00001001. exiting gracefully' \
	'.00001001. free env 00001001'

runtest1 pingpong \
	'.00000000. new env 00001000' \
	'.00000000. new env 00001001' \
	'.00001001. new env 00001002' \
	'send 0 from 1001 to 1002' \
	'1002 got 0 from 1001' \
	'1001 got 1 from 1002' \
	'1002 got 8 from 1001' \
	'1001 got 9 from 1002' \
	'1002 got 10 from 1001' \
	'.00001001. exiting gracefully' \
	'.00001001. free env 00001001' \
	'.00001002. exiting gracefully' \
	'.00001002. free env 00001002' \

timeout=20
runtest1 primes \
	'.00000000. new env 00001000' \
	'.00000000. new env 00001001' \
	'.00001001. new env 00001002' \
	'2 .00001002. new env 00001003' \
	'3 .00001003. new env 00001004' \
	'5 .00001004. new env 00001005' \
	'7 .00001005. new env 00001006' \
	'11 .00001006. new env 00001007' 

echo "Part C score: $score/15"
echo
total=`expr $total + $score`



echo "Score: $total/65"

if [ $total -lt 65 ]; then
    exit 1
fi
