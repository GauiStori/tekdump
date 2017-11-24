/* 
 * tekdump.c - convert screen dumps from Tektronix 11801a osciliscope
 *	into a useful format.
 *
 * By Steve Tell, May 23, 1997
 *	
 */

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tty.h"

static char *prog_version = "tekdump v0.1 23-May-1997";
static char *progname;
int g_verbose = 0;
int m_flag = 0;
int h_flag = 0;
FILE *g_rawfile;

void do_file(FILE *fp);
extern int do_decode(FILE *infp, FILE *outfp,
		     int B_flag, int U_flag, int nrows, int ncols);
extern int get_header(FILE *infp, 
	       int dtlen, char *date, char *time, int *nrowsp, int *ncolsp);

int do_server(char *line, int baud, char *prefix, char *fprog, char *suffix,
	  int B_flag, int U_flag);
int do_onefile(FILE *infp, FILE *outfp, int B_flag, int U_flag);

void usage()
{
	fprintf(stderr, "Usage: %s [options] <prefix>\n", progname);
	fprintf(stderr, "  convert screen dump images from 11801 scope to useful format\n");
	fprintf(stderr, "  Unless -f file specified, operates as a server, listening for serial data.\n");
	fprintf(stderr, "  In server mode, writes to numbered files <prefix>NNN.<type>\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-B           data is binary\n");
	fprintf(stderr, "\t-U           data is uncompressed\n");
	fprintf(stderr, "\t-b  <baud>   receive from scope at <baud> bps\n");
	fprintf(stderr, "\t-l  <device> tty to receive from scope\n");
	fprintf(stderr, "\t-f  <file>   process named file; -f - for stdin\n");
	fprintf(stderr, "\t-m           map colors to monochrome\n");
	fprintf(stderr, "\t-t  <type>   write <type> file instead of ppm; type=gif, tiff, or gzip\n");
	fprintf(stderr, "\t-v           verbose\n");
	fprintf(stderr, "Version: %s\n", prog_version);
}

int
main(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;
	char *do_infile;
	char *fprog = NULL;
	FILE *fin;
	char *fsuffix = "ppm";
	char *tty_name = "/dev/cua0";	/* defaults */
	int tty_baud = 19200;
	int errflg = 0;
	int U_flag = 0;
	int B_flag = 0;
	int c;
	int svrargs = 0;
	int rc;

	progname = argv[0];
	while ((c = getopt (argc, argv, "BUb:f:hl:mvx:t:")) != EOF) {
		switch(c) {
		case 'B':
			B_flag = 1;
			break;
		case 'U':
			U_flag = 1;
			break;
		case 'b':
		    	tty_baud = atoi(optarg);
			svrargs = 1;
			break;
		case 'f':
		    	do_infile = optarg;
			break;
		case 'h':
		    	h_flag = 1;
			break;
		case 'l':
		    	tty_name = optarg;
			svrargs = 1;
			break;
		case 'm':
			m_flag = 1;
			break;
		case 't':
			svrargs = 1;
			if(0==strcmp(optarg, "gif")) {
				fprog = "ppmtogif";
				fsuffix = "gif";
			} else if(0==strcmp(optarg, "tiff")) {
				fprog = "pnmtotiff";
				fsuffix = "tiff";
			} else if(0==strcmp(optarg, "gzip")) {
				fprog = "gzip";
				fsuffix = "ppm.gz";
			} else {
				fprintf(stderr, "%s: unknown output type %s\n",
					progname, optarg);
				errflg = 1;
			}
			
			break;
		case 'v':
			g_verbose = 1;
			break;
		case 'x':
			g_rawfile = fopen(optarg, "w");
			break;
		default:
			errflg = 1;
			break;
		}
	}

	if(errflg) {
		usage();
		exit(1);
	}
	if(svrargs && do_infile) {
		fprintf(stderr, "%s: -f option cannot be used with -b, -l or -t\n", progname);
		exit(1);
	}

	rc = 0;
	if(do_infile) {
		if(0==strcmp(do_infile, "-")) {
			fin = stdin;
		} else {
			fin = fopen(do_infile, "r");
		}
		if(fin) {
			rc = do_onefile(fin, stdout, B_flag, U_flag);
			fclose(fin);
		} else {
			perror(argv[optind]);
		}
	} else {
		if(optind == argc) {
			fprintf(stderr, "no file prefix specfied\n");
			rc = -1;
		} else {
			rc = do_server(tty_name, tty_baud, argv[optind],
				       fprog, fsuffix, B_flag, U_flag);
		}
	}

	if(rc < 0)
		exit(1);
	else
		exit(0);
}

int
do_onefile(FILE *infp, FILE *outfp, int B_flag, int U_flag)
{
	int ncols, nrows;
	char date[64];
	char time[64];

	if(get_header(infp, 64, date, time, &nrows, &ncols) < 0)
		return -1;
	return do_decode(infp, stdout, B_flag, U_flag, nrows, ncols);
}

int
do_server(char *line, int baud, char *prefix, char *filter, char *suffix,
	  int B_flag, int U_flag)
{
	int tty_fd;
	FILE *fin, *outfp;
	int dumpno;
	int ncols, nrows;
	char date[64];
	char time[64];
	char fname[1024];
	int rc;

	tty_fd = tty_open(line); /* open, print error message if failure */
	if(tty_fd < 0)
		return -1;
	if(tty_set(tty_fd, baud, 8, 1, NO_PARITY) < 0)
		return -1;

	fin = fdopen(tty_fd, "r");
	if(!fin) {
		fprintf(stderr, "%s: can't fdopen.\n", line);
		return -1;
	}
	if(g_verbose)
		fprintf(stderr, "listening on %s at %d bps\n", line, baud);

	for(dumpno = 0; ; dumpno++) {
		if(get_header(fin, 64, date, time, &nrows, &ncols) < 0)
			return -1;

		if(filter) {
			sprintf(fname, "%s>%s%d.%s", 
				filter, prefix, dumpno, suffix);
			outfp = popen(fname, "w");
		} else {
			sprintf(fname, "%s%d.ppm", prefix, dumpno);
			outfp = fopen(fname, "w");
		}
		if(!outfp) {
			perror(fname);
			return -1;
		}
		if(g_verbose) {
			fprintf(stderr, "writing image to %s\n", fname);
		} else {
			fprintf(stderr, "%s %s writing file %s\n", date, time, fname);
		}
		
		rc = do_decode(fin, outfp, B_flag, U_flag, nrows, ncols);

		if(filter)
			pclose(outfp);
		else
			fclose(outfp);
		if(rc<0)
			return -1;

		if(!h_flag)
			fprintf(stderr, "done writing %s\n", fname);
	}
}

