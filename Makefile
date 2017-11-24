
HTMLDIR=/afs/unc/home/msl/man/src_html
MANDIR=/usr/msl/vlsi/man
BINDIR=/usr/msl/vlsi/bin

CC=gcc
LD=gcc

CFLAGS=-g

all: tekdump trt tekdump.1 tekdump.html

tekdump: tekdump.o decode.o tty.o
	$(LD) $(LDFLAGS) -o $@ $^

trt: trt.o tty.o
	$(LD) -o $@ $^

tty.o: tty.h
tekdump.o: tty.h
trt.o: tty.h

clean:
	rm -f tekdump trt *.o tekdump.1 tekdump.html

#
# This isn't quite as clean as it looks.
# We want to have screen shots of the setup menus in the HTML version,
# and pod2html has a suitable (but undocumented) mechanism to let us do this.
# but pod2man doesn't strip out the html bits.
#
tekdump.1: tekdump.pod
	pod2man --section=1 --center="MSL local utilities" --release=MSL $^ > $@

POD2HTML=./pod2html-1.15
tekdump.html: tekdump.pod
	$(POD2HTML) $^

