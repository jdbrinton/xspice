#
# Makefile for Simulator directories
#

# Include global XSPICE selections for CC and other macros
include /usr/local/xspice-1-0/include/make.include

OBJLIB = $(ROOT)/lib/sim/object
SRCLIB = $(ROOT)/lib/sim/source

INCLUDE = -I. -I$(ROOT)/include/sim -I/usr/include/X11/Xaw -I/usr/include/X11
LIB = -L/usr/X11/lib -lXaw -lXt -lXmu -lX11 -lm -ltermlib
CFLAGS = -g

.c.o: $*.c
	${CC} ${CFLAGS} ${INCLUDE} -c $*.c

xspice: force
	@echo
	@echo "Running preprocessor on modpath.lst and udnpath.lst ..."
	$(BINDIR)/cmpp -lst
	@echo
	@echo "Compiling list of models and node types..."
	$(CC)  -o temp.o $(CFLAGS) $(INCLUDE) -c $(SRCLIB)/SPIinit.c
	@echo "$(CC) -o xspice temp.o \\" > make.tmp
	@echo "$(OBJLIB)/core.o $(LIB) \\" >> make.tmp
	@cat objects.inc >> make.tmp
	@echo
	@echo "Linking XSPICE simulator ..."
	@cat make.tmp
	@chmod 777 make.tmp
	@make.tmp
	@echo
	@echo "Copying executable to /usr/local/xspice-1-0/bin ..."
	@cp xspice $(ROOT)/bin/xspice
	@echo
	@echo "Deleting temporary files ..."
	@-rm make.tmp temp.o CMinfo.h CMextrn.h UDNinfo.h UDNextrn.h objects.inc xspice
	@echo
	@echo
	@echo "XSPICE simulator created and installed."
	@echo
	@echo "Type: \"/usr/local/xspice-1-0/bin/xspice <input deck>\" to run."
	@echo
	@echo "See directory /usr/local/xspice-1-0/lib/sim/examples for sample decks."
	@echo

force:
	@true
