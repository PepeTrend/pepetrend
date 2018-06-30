# [CTFN-3G-001] Susan: Support 3G feature. CT-5364A_CTL_3G, added Makefile, 2009/06/30
#	$Id: Makefile.linux,v 1.9 1999/08/13 01:54:32 paulus Exp $

CDEF1=	-DTERMIOS			# Use the termios structure
CDEF2=	-DSIGTYPE=void			# Standard definition
CDEF3=	-UNO_SLEEP			# Use the usleep function
CDEF4=	-DFNDELAY=O_NDELAY		# Old name value
CDEFS=	$(CDEF1) $(CDEF2) $(CDEF3) $(CDEF4)

CFLAGS=	-O2 -g -pipe $(CDEFS)

INSTALL= install

all:	chat

chat:	chat.o
	$(CC) -o chat chat.o

chat.o:	chat.c
	$(CC) -c $(CFLAGS) -o chat.o chat.c
dynamic: chat install

install:
	install -m 755 chat $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/chat

#install: chat
#	mkdir -p $(BINDIR)
#	$(INSTALL) -s -c chat $(BINDIR)
#	$(INSTALL) -c -m 644 chat.8 $(MANDIR)/man8

clean:
	rm -f chat.o chat *~
