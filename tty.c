/*
 * General-purpose serial I/O routines.  
 * Oriented towards talking to modems and embedded devices. 
 * by Steve Tell
 *
 * $Log: tty.c,v $
 * Revision 1.1  1997/05/22 04:13:57  tell
 * Initial revision
 *
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/* this should probably go in a config.h file or somthing like that */
#ifdef hpux
#define NEED_MODEM_H
#endif

#ifdef NEED_MODEM_H
#include <sys/modem.h>
#endif
#include <termios.h>

#include "tty.h"

extern int g_verbose;

typedef struct _tty_speed
{
	int code;
	int speed;
	
} tty_speed;

tty_speed tty_speeds[] =
{
	{B50, 50},
	{B75, 75},
	{B110, 110},
	{B134, 134},
	{B150, 150},
	{B200, 200},
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B1800, 1800},
	{B2400, 2400},
	{B4800, 4800},
	{B9600, 9600},
	{B19200, 19200},
	{B38400, 38400},
#ifdef B57600
	{B57600, 57600},
#endif
#ifdef B115200
	{B115200, 115200},
#endif
#ifdef B230400
	{B230400, 230400},
#endif
#ifdef B460800
	{B460800, 460800},
#endif
	{-1, -1}
};


typedef struct _tty_size
{
	int code;
	int size;
	
} tty_size;

tty_size tty_sizes[] =
{
	{CS5, 5},
	{CS6, 6},
	{CS7, 7},
	{CS8, 8},
	{-1, -1}
};

/*
 * Open serial terminal device.  
 * Will not block opening the device, but subsequently arranges for reads
 * to block.
 */
int tty_open(char *device)
{
	int fd;
	
	/* set O_NDELAY so open() returns without waiting for carrier */
	if((fd = open(device, O_RDWR | O_NDELAY | O_NOCTTY, 0)) < 0)
	{
		perror(device);
		return -1;
	}								   
	
	if (tty_blocking(fd) < 0)
	{
		fprintf(stderr, "setting blocking on %s: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

void tty_close(int fd)
{
	/* tty_raw(fd); */
	close(fd);
}

int tty_isspeed(int speed)
{
	tty_speed *s;

	for (s = &tty_speeds[0]; s->code != -1; s++) {
		if (s->speed == speed) {
			return s->code;
		}
	}
	return -1;
}

int tty_set(int fd, int speed, int data, int stop, int parity)
{
	int scode;
	tty_speed *s;
	tty_size *d;
	struct termios t;

	if (tcgetattr(fd, &t) < 0) {
		fprintf(stderr, "can not get termios\n");
		return -1;
	}

	 /* set up the modes that we need */
	t.c_cflag = CREAD | CLOCAL | HUPCL;
#ifdef CRTSCTS
	t.c_cflag |= CRTSCTS;
#endif
	t.c_iflag |= IXOFF;
	t.c_oflag = 0;
	t.c_lflag = 0;

	t.c_cc[VMIN]  = 128;
	t.c_cc[VTIME] = 5;

	scode = tty_isspeed(speed);
	if (scode == -1) {
		fprintf(stderr, "speed %d not supported\n", speed);
		return -1;
	}
	cfsetispeed(&t, scode);
	cfsetospeed(&t, scode); 

	for (d = tty_sizes; d->size != data; d++) {
		if (d->code == -1) {
			fprintf(stderr, "%d data bits not supported\n", data);
			return -1;
		}
	}
	t.c_cflag |= d->code;

	switch (stop) {
	case 1:
		break;
	case 2:
		t.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "%d stop bits not supported\n", stop);
		return -1;
	}

	switch (parity)	{
	case NO_PARITY:
		break;
	case ODD_PARITY:
		t.c_cflag |= PARENB;
	case EVEN_PARITY:
		t.c_cflag |= PARODD;
		break;
	default:
		fprintf(stderr, "parity must be odd, even or none\n");
		return -1;
	}

	if (tcsetattr(fd, TCSANOW, &t) < 0) {
		fprintf(stderr, "can not set termios\n");
		return -1;
	}

	return 0;
}

int tty_blocking(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
	     fcntl(fd, F_SETFL, flags & ~O_NDELAY) < 0)
		return -1;
	return 0;
}

int tty_flush(int fd)
{
	tcdrain(fd);
	return 0;
}


/*
 * wait for character.
 * uses alarm(3).  Caller must set up signal handler if desired.
 */
int tty_waitforchar(int fd, char c, int timeout, int echo)
{
	char buf[256];
	int n;
	alarm(timeout);
	do {
		n = read(fd, buf, 256);
		if(n == -1) {
			perror("waitforchar: read");
			return -1;
		}
		if(echo) {
			fwrite(buf, 1, n, stdout);
			fflush(stdout);
		}
	} while(memchr(buf, c, n) == NULL);
	alarm(0);
	return 0;
}

/*
 * Read a "line."  Can use any character as the end-of-line indicator.
 * Again, if a timeout is desired, the caller can catch SIGALARM.
 * If the line is longer than the user's buffer length, excess is discarded.
 */
int tty_getline(int fd, char *buf, int len, char term, int timeout, int echo)
{
	char rbuf[4];
	int n, nr;
	nr = 0;
	alarm(timeout);
	do {
		n = read(fd, rbuf, 1);
		if(n == -1) {
			perror("getline: read");
			return -1;
		}
		if(echo) {
			fwrite(buf, 1, n, stdout);
			fflush(stdout);
		}
		if(nr < len) {
			buf[nr++] = rbuf[0];
		}
	} while(rbuf[0] != term);
	alarm(0);
	return 0;
}

void
tty_rtson(int fd)
{
	int rts = TIOCM_RTS;
	ioctl(fd, TIOCMBIS, &rts);
}

void
tty_rtsoff(int fd)
{
	int rts = TIOCM_RTS;
	ioctl(fd, TIOCMBIC, &rts);
}
