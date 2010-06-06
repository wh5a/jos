#!/bin/sh

qemuopts="-hda obj/kern/kernel.img -hdb obj/fs/fs.img"
. ./grade-functions.sh

$make

rand() {
	perl -e "my \$r = int(1024 + rand() * (65535 - 1024));print \"\$r\\n\";"
}

qemu_test_httpd() {
	pts=5

	echo ""

	perl -e "print '    wget localhost:$http_port/: '"
	if wget -o wget.log -O /dev/null localhost:$http_port/; then
		echo "WRONG, got back data";
	else
		if egrep "ERROR 404" wget.log >/dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, did not get 404 error";
		fi
	fi

	perl -e "print '    wget localhost:$http_port/index.html: '"
	if wget -o /dev/null -O qemu.out localhost:$http_port/index.html; then
		if diff qemu.out fs/index.html > /dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, returned data does not match index.html";
		fi
	else
		echo "WRONG, got error";
	fi

	perl -e "print '    wget localhost:$http_port/random_file.txt: '"
	if wget -o wget.log -O /dev/null localhost:$http_port/random_file.txt; then
		echo "WRONG, got back data";
	else
		if egrep "ERROR 404" wget.log >/dev/null; then
			score=`expr $pts + $score`
			echo "OK";
		else
			echo "WRONG, did not get 404 error";
		fi
	fi

	kill $qemu_pid
	wait 2> /dev/null

	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"
}

qemu_test_echosrv() {
	pts=85

	str="$t0: network server works"
	echo $str | nc -q 3 localhost $echosrv_port > qemu.out

	kill $qemu_pid
	wait 2> /dev/null

	t1=`date +%s.%N 2>/dev/null`
	time=`echo "scale=1; ($t1-$t0)/1" | sed 's/.N/.0/g' | bc 2>/dev/null`
	time="(${time}s)"

	if egrep "^$str\$" qemu.out > /dev/null
	then
		score=`expr $pts + $score`
		echo OK $time
	else
		echo WRONG $time
	fi
}

# Override run to start QEMU and return without waiting
run() {
	t0=`date +%s.%N 2>/dev/null`
	# The timeout here doesn't really matter, but it helps prevent
	# runaway qemu's
	(
		ulimit -t $timeout
		exec $qemu -nographic $qemuopts -serial file:jos.out -monitor null -no-reboot
	) >$out 2>$err &
        qemu_pid=$!

	sleep 8 # wait for qemu to start up
}

# Make continuetest a no-op and run the tests ourselves
continuetest () {
	return
}

# Reset the file system to its original, pristine state
resetfs() {
	rm -f obj/fs/fs.img
	$make obj/fs/fs.img >$out
}

score=0

http_port=`rand`
echosrv_port=`rand`
echo "using http port: $http_port"
echo "using echo server port: $echosrv_port"

qemuopts="$qemuopts -net user -net nic,model=i82559er"
qemuopts="$qemuopts -redir tcp:$echosrv_port::7 -redir tcp:$http_port::80"

resetfs

runtest1 -tag 'tcp echo server [echosrv]' echosrv
qemu_test_echosrv

runtest1 -tag 'web server [httpd]' httpd
qemu_test_httpd

echo "Score: $score/100"

if [ $score -lt 100 ]; then
    exit 1
fi
