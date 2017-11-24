/* 
 * Dump raw data from 11801A to stdout
 */
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "tty.h"

static char *prog_version = "trt v0.1 23-May-1997";
static char *progname;
int g_verbose = 0;

int
main(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	char *tty_name = "/dev/cua0";	/* defaults */
	int tty_baud = 19200;
	int errflg = 0;
	int c;
	int rc;
	int tty_fd;
	char buf[256];
	int n;

	progname = argv[0];
	while ((c = getopt (argc, argv, "b:lv")) != EOF) {
		switch(c) {
		case 'b':
		    	tty_baud = atoi(optarg);
			break;
		case 'l':
		    	tty_name = optarg;
			break;
		case 'v':
			g_verbose = 1;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	tty_fd = tty_open(tty_name); /* open, print error message if failure */
	if(tty_fd < 0)
		exit(1);
	if(tty_set(tty_fd, tty_baud, 8, 1, NO_PARITY) < 0)
		exit(1);

	for(;;) {
		n = read(tty_fd, buf, 256);
		if(n>0) {
			write(1, buf, n);
		}
	}
}

