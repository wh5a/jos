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
		$qemu -nographic $qemuopts -serial null -parallel file:jos.out -s -S -gdb tcp::$port
	) >$out 2>$err &

	(
		echo "target remote localhost:$port"
                if [ "x$qemuphys" = "x" ]; then
		    echo "br *0x$brkaddr"
                else
		    echo "br *(0x$brkaddr-0xf0000000)"
                fi
		echo c
	) > jos.in

	gdb -batch -nx -x jos.in > /dev/null
	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"
	rm jos.in
}
