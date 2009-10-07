verbose=false

if [ "x$1" = "x-v" ]
then
	verbose=true
	out=/dev/stdout
	err=/dev/stderr
else
	out=/dev/null
	err=/dev/null
fi

if gmake --version >/dev/null 2>&1; then make=gmake; else make=make; fi

pts=5
timeout=30
preservefs=n
qemu=`$make -s --no-print-directory which-qemu`

echo_n () {
	# suns can't echo -n, and Mac OS X can't echo "x\c"
	# assume argument has no doublequotes
	awk 'BEGIN { printf("'"$*"'"); }' </dev/null
}

run () {
	# Find the address of the kernel readline function,
	# which the kernel monitor uses to read commands interactively.
	brkaddr=`grep 'readline$' obj/kern/kernel.sym | sed -e's/ .*$//g'`
	#echo "brkaddr $brkaddr"

	# Generate a unique GDB port
	port=$(expr `id -u` % 5000 + 25000)

	# Run qemu, setting a breakpoint at readline(),
	# and feeding in appropriate commands to run, then quit.
	t0=`date +%s.%N 2>/dev/null`
	(
		ulimit -t $timeout
		exec $qemu -nographic $qemuopts -serial file:jos.out -monitor null -no-reboot -s -S -p $port
	) >$out 2>$err &
	PID=$!

	(
		echo "target remote localhost:$port"
		echo "br *0x$brkaddr"
		echo c
	) > jos.in

	sleep 1
	gdb -batch -nx -x jos.in > /dev/null
	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"
	rm jos.in

	# Make sure QEMU is dead.  On OS X, exiting gdb doesn't always exit QEMU.
	kill $PID > /dev/null 2>&1
}

# Usage: runtest <tagname> <defs> <strings...>
runtest () {
	perl -e "print '$1: '"
	rm -f obj/kern/init.o obj/kern/kernel obj/kern/kernel.img 
	[ "$preservefs" = y ] || rm -f obj/fs/fs.img
	if $verbose
	then
		echo "$make $2... "
	fi
	$make $2 >$out
	if [ $? -ne 0 ]
	then
		rm -f obj/kern/init.o
		echo $make $2 failed 
		exit 1
	fi
	# We just built a weird init.o that runs a specific test.  As
	# a result, 'make qemu' will run the last graded test and
	# 'make clean; make qemu' will run the user-specified
	# environment.  Remove our weird init.o to fix this.
	rm -f obj/kern/init.o
	run
	if [ ! -s jos.out ]
	then
		echo 'no jos.out'
	else
		shift
		shift
		continuetest "$@"
	fi
}

quicktest () {
	perl -e "print '$1: '"
	shift
	continuetest "$@"
}

stubtest () {
    perl -e "print qq|$1: OK $2\n|";
    shift
    score=`expr $pts + $score`
}

continuetest () {
	okay=yes

	not=false
	for i
	do
		if [ "x$i" = "x!" ]
		then
			not=true
		elif $not
		then
			if egrep "^$i\$" jos.out >/dev/null
			then
				echo "got unexpected line '$i'"
				if $verbose
				then
					exit 1
				fi
				okay=no
			fi
			not=false
		else
			egrep "^$i\$" jos.out >/dev/null
			if [ $? -ne 0 ]
			then
				echo "missing '$i'"
				if $verbose
				then
					exit 1
				fi
				okay=no
			fi
			not=false
		fi
	done
	if [ "$okay" = "yes" ]
	then
		score=`expr $pts + $score`
		echo OK $time
	else
		echo WRONG $time
	fi
}

# Usage: runtest1 [-tag <tagname>] <progname> [-Ddef...] STRINGS...
runtest1 () {
	if [ $1 = -tag ]
	then
		shift
		tag=$1
		prog=$2
		shift
		shift
	else
		tag=$1
		prog=$1
		shift
	fi
	runtest1_defs=
	while expr "x$1" : 'x-D.*' >/dev/null; do
		runtest1_defs="DEFS+='$1' $runtest1_defs"
		shift
	done
	runtest "$tag" "DEFS='-DTEST=_binary_obj_user_${prog}_start' DEFS+='-DTESTSIZE=_binary_obj_user_${prog}_size' $runtest1_defs" "$@"
}

