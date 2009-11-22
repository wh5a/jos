#include <inc/lib.h>

void wrong(int, int, int);

void
umain(void)
{
	char c1, c2;
	int r, rfd, wfd, kfd, n1, n2, off, nloff;

	close(0);
	close(1);
	opencons();
	opencons();

	if ((rfd = open("testshell.sh", O_RDONLY)) < 0)
		panic("open testshell.sh: %e", rfd);
	if ((wfd = open("testshell.out", O_WRONLY)) < 0)
		panic("open testshell.out: %e", wfd);

	cprintf("running sh -x < testshell.sh > testshell.out\n");
	if ((r = fork()) < 0)
		panic("fork: %e", r);
	if (r == 0) {
		dup(rfd, 0);
		dup(wfd, 1);
		close(rfd);
		close(wfd);
		if ((r = spawnl("/sh", "sh", "-x", 0)) < 0)
			panic("spawn: %e", r);
		close(0);
		close(1);
		wait(r);
		exit();
	}
	close(rfd);
	close(wfd);
	wait(r);

	if ((rfd = open("testshell.out", O_RDONLY)) < 0)
		panic("open testshell.out for reading: %e", rfd);
	if ((kfd = open("testshell.key", O_RDONLY)) < 0)
		panic("open testshell.key for reading: %e", kfd);

	nloff = 0;
	for (off=0;; off++) {
		n1 = read(rfd, &c1, 1);
		n2 = read(kfd, &c2, 1);
		if (n1 < 0)
			panic("reading testshell.out: %e", n1);
		if (n2 < 0)
			panic("reading testshell.key: %e", n2);
		if (n1 == 0 && n2 == 0)
			break;
		if (n1 != 1 || n2 != 1 || c1 != c2)
			wrong(rfd, kfd, nloff);
		if (c1 == '\n')
			nloff = off+1;
	}
	cprintf("shell ran correctly\n");			
}

void
wrong(int rfd, int kfd, int off)
{
	char buf[100];
	int n;

	seek(rfd, off);
	seek(kfd, off);

	cprintf("shell produced incorrect output.\n");
	cprintf("expected:\n===\n");
	while ((n = read(kfd, buf, sizeof buf-1)) > 0)
		sys_cputs(buf, n);
	cprintf("===\ngot:\n===\n");
	while ((n = read(rfd, buf, sizeof buf-1)) > 0)
		sys_cputs(buf, n);
	cprintf("===\n");
	exit();
}

