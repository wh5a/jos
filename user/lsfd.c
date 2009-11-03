#include <inc/lib.h>

void
usage(void)
{
	cprintf("usage: lsfd [-1]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i, usefprint = 0;
	struct Stat st;

	ARGBEGIN{
	default:
		usage();
	case '1':
		usefprint = 1;
		break;
	}ARGEND

	for (i = 0; i < 32; i++)
		if (fstat(i, &st) >= 0) {
			if (usefprint)
				fprintf(1, "fd %d: name %s isdir %d size %d dev %s\n",
					i, st.st_name, st.st_isdir,
					st.st_size, st.st_dev->dev_name);	
			else
				cprintf("fd %d: name %s isdir %d size %d dev %s\n",
					i, st.st_name, st.st_isdir,
					st.st_size, st.st_dev->dev_name);
		}
}
