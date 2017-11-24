#ifndef _TTY_H
#define _TTY_H


#include <termios.h>


#define NO_PARITY		0
#define EVEN_PARITY		1
#define ODD_PARITY		2

#define CTL(x)		(x ^ 0100)

#define WAIT_HANGUP	5

int tty_isspeed(int speed);
int tty_open(char *tty);
void tty_close(int fd);
int tty_set(int fd, int speed, int data, int stop, int parity);
int tty_raw(int fd);
int tty_cooked(int fd);
int tty_hangup(int fd);
int tty_discard(int fd);
int tty_blocking(int fd);
int tty_flush(int fd);
int tty_ctty(int fd);
void tty_own(char *tty);
int tty_softcar(int fd, int soft);

#endif
