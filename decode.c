/*
 * routines to convert hex and binary forms of Tektronix 11801a
 * screendump data to PPM format.
 *
 * Steve Tell, May 23, 1997.
 */

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <assert.h>

extern int g_verbose;
extern int h_flag;
extern int m_flag;
extern FILE *g_rawfile;

void write_pixel(int p, FILE *outfp);
char *fgetline(char *buf, int n, FILE *fp);

/* 8-entry colormaps.  
 * First, for full-color images that look like the 11801's screen
 */

typedef struct {
  unsigned char r, g, b;
} Color;

/* These are the default colors on the Tektronix 11801A,
 * sort of tweaked by hand
 */
Color color_map[] = {
	{  0,   0,   0},	/* black - background */
	{255, 255, 200},	/* pink - trace color 2 */
	{255, 130, 130},	/* pale yellow - trace color 1, misc text */
	{  0, 255,   0},	/* green - trace color 3 */

	{255, 000, 255},	/* magenta - trace color 4 */
	{  0, 255, 255},	/* cyan - window trace */
	{140, 140, 140},	/* grey - graticules, selectorss */
	{255,   0,   0},	/* red  - cursors, measurement zones */
};

/*
 * Next, for black-on-white images for use in a document when the scope
 * was set for color mode.  This is a compromise mapping; some of the
 * settings at the bottom are usually illegible, but the traces are usually OK.
 * 
 * For better results, either adjust the scope's pallette and use color mode,
 * or use a paint program to map the color dump's colors more intelligently.
 */
int bw_map[] = {
  0,
  1,
  1,
  1,

  1,
  1,
  1,
  1
};


/*
 * Routine to get header from image dump
 */
int get_header(FILE *infp, 
	       int dtlen, char *date, char *time, int *nrowsp, int *ncolsp)
{
	char buf[2048];
	int n;
	char *cp;
	memset(buf, 0, 2047);

	if(fgetline(buf, 2048, infp) == NULL) {
		fprintf(stderr, "EOF reading header\n");
		return -1;
	}
	if(strlen(buf) < 20) {
		fprintf(stderr, "short header: [%d] %s\n", strlen(buf), buf);
		return -1;
	}
	if(cp = strstr(buf, "date:")) {
		n = strcspn(&cp[6], " \t\n\r");
		strncpy(date, &cp[6], n);
		date[n] = 0;
	} else {
		fprintf(stderr, "bad header: no date\n");
		return -1;
	}
	if(cp = strstr(buf, "time:")) {
		n = strcspn(&cp[6], " \t\n\r");
		strncpy(time, &cp[6], n);
		time[n] = 0;
	}
	if(fgetline(buf, 2048, infp) == NULL) {
		fprintf(stderr, "EOF reading header\n");
		return -1;
	}
	*ncolsp = atoi(buf);
	if(fgetline(buf, 2048, infp) == NULL) {
		fprintf(stderr, "EOF reading header\n");
		return -1;
	}
	*nrowsp = atoi(buf);

	if(g_verbose) {
		fprintf(stderr, "header: date=%s time=%s ncols=%d nrows=%d\n",
			date, time, *ncolsp, *nrowsp);
	}
}

/*
 * decode an image while copying from one stdio stream to another.
 * header must have already been read, and the correct number of
 * rows and columns passed in.
 * We stop reading at the right point based on the number of rows and columns.
 */
int
do_decode(FILE *infp, FILE *outfp,
	  int B_flag, int U_flag, int nrows, int ncols)
{
	int c;
	int n;
	int rw, cl;
	char buf[2048];
	char *cp;
	int per, oper;

	if(B_flag) {	/* NULL ends header in binary mode */
		while((c = getc(infp)) != EOF) {
			if(c == '\0')
				break;
		}
		if(c == EOF) {
			fprintf(stderr, "EOF reading header\n");
			return -1;
		}
	}

	/* write PBM or PPM output header */
	if(m_flag) {
		fprintf(outfp, "P1\n");
		fprintf(outfp, "%d %d\n", ncols, nrows);
	} else {
		fprintf(outfp, "P3\n");
		fprintf(outfp, "%d %d\n", ncols, nrows);
		fprintf(outfp, "255\n");
	}

	if(g_verbose)
		fprintf(stderr, "processing body\n");
	/* process by rows */
	oper = per = 0;
	for(rw = 1; rw <= nrows; rw++) {

		if(B_flag) {
			if(do_row_bin(infp, outfp, ncols, U_flag, rw) < 0)
				return -1;
		} else {
			if(do_row_hex(infp, outfp, ncols, U_flag, rw) < 0)
				return -1;
		}
		if(h_flag) {  /* always print 50 hash marks per conversion */
			per = (rw*50)/nrows;
			if(per != oper) {
				fputc('#', stderr);
				fflush(stderr);
			}
			oper = per;
		}
	}
	if(h_flag)
		fputc('\n', stderr);
	if(g_rawfile)
		fflush(g_rawfile);
	return 0;
}

int
do_row_hex(FILE *infp, FILE *outfp, int ncols, int U_flag, int row)
{
	int c;
	int i, rpt, j, len;
	char buf[2048];
	char line[1024];
	char *cp;
	int nc;
	int b, b2, b3;

	memset(line, 7, 1024);/* make decode errors obvious */

	do { /* read, skipping blank lines */
		if(fgetline(buf, 2048, infp) == NULL) {
			if(h_flag)
				fputc('\n', stderr);
			fprintf(stderr, "EOF reading body at row %d\n", row);
			return -1;
		}
		if(cp = strchr(buf, '\n')) {
			*cp = 0;
		}
	} while((len = strlen(buf)) < 1);
	
	if(U_flag) {
		nc = 0;
		for(i = 1; i < len; i += 2) {
			c = buf[i];
			if(c < '0' || c > '7') {
				if(h_flag)
					fputc('\n', stderr);
				fprintf(stderr, "bad char in body at row %d\n", row);
				/*return -1;*/
			}
			line[nc++] = (b&0x7);
		}
	} else {
		nc = 0;
		i = 0;
/*		fprintf(stderr, "line (len=%d): \"%s\"\n", len, buf); */
		while(i < len) {
			b = getHex(&buf[i]);
			if(b < 0) {
				if(h_flag)
					fputc('\n', stderr);
				fprintf(stderr, "bad chars '%c%c' in body at row %d\n", buf[i], buf[i+1], row);
				/* return -1;*/
			}
			switch(b & 0xC0) {
			case 0x40:
				rpt = 1;
				break;
			case 0x80:
				rpt = 2;
				break;
			case 0xC0:
				rpt = 3;
				break;
			case 0x00:
				i+= 2;
				b2 = getHex(&buf[i]);
				if(b2 < 0) {
					if(h_flag)
						fputc('\n', stderr);
					fprintf(stderr, "bad chars '%c%c' in body at row %d\n", row, buf[i], buf[i+1]);
					/*return -1; */
				}
				if(b2 >= 4) {
					rpt = b2;
				} else {
					i+= 2;
					b3 = getHex(&buf[i]);
					if(b3 < 0) {
						if(h_flag)
							fputc('\n', stderr);
						fprintf(stderr, "bad chars '%c%c' in body at row %d\n", row, buf[i], buf[i+1]);
						/*return -1; */
					}
					rpt = b2<<8|b3;
				}
				break;
			}
/*			fprintf(stderr, "  rpt=%d bits=%02o\n", rpt, b&0x3f); */
			for(j = 0; j < rpt; j++) {
				line[nc++] = (b&0x7);
				line[nc++] = (b&0x38)>>3;
				if(nc > ncols || nc >= 1024) {
					if(h_flag)
						fputc('\n', stderr);
					fprintf(stderr, "decoding error; line length too long at row %d\n", row);
					goto break_row; /* break row; */
				}
			}
			i+= 2;
		}
	break_row:
	}
	if(nc != ncols) {
		if(h_flag)
			fputc('\n', stderr);
		fprintf(stderr, "expected %d columns, got %d at row %d\n",
			ncols, nc, row);
	}
	/* write a row of the expected length, even if there were errors */
	for(i = 0; i < ncols; i++) {
		write_pixel(line[i], outfp);
	}

	fputc('\n', outfp);

	return 0;
}

int getNib(int c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
}

int getHex(char *s)
{
	if(!isxdigit(s[0])) return -1;
	if(!isxdigit(s[1])) return -1;
	return getNib(s[0]) << 4 | getNib(s[1]);
}

void write_pixel(int p, FILE *outfp)
{
	assert(p >= 0 && p <= 7);

	if(m_flag) {
		fprintf(outfp, "%d", bw_map[p]);
	} else {
		fprintf(outfp, "%d %d %d ", 
			color_map[p].r,
			color_map[p].g,
			color_map[p].b);
	}
}



int
do_row_bin(FILE *infp, FILE *outfp, int ncols, int U_flag, int row)
{
	fprintf(stderr, "binary not implemented yet\n");
	return -1;
}

/*
 * fgetline - similar to fgets, except skips over blank lines
 * and control characters.
 *
 * result is undefined if n <= 1.
 *
 * Always reads a whole line of input.
 * Discards the tail of lines that are too long for the buffer.
 */

char *fgetline(char *buf, int n, FILE *fp)
{
	int c;
	int idx = 0;
	n--;           /* leave room for final NULL */
	buf[0] = 0;

	while((c = getc(fp)) != EOF) {
		if(g_rawfile)
			putc(c, g_rawfile);
		if((c == '\n' || c == '\r') && idx>0) { 
			/* newline  or CR on non-empty line */
			if(n) {
				buf[idx++] = '\n';
				buf[idx] = 0;
			}
			return buf;
		} else if(!iscntrl(c)) {
			if(n) {
				buf[idx++] = c;
				buf[idx] = 0;
				n--;
			}
			
		}
	}
	/* hit EOF */
	return NULL;
}







