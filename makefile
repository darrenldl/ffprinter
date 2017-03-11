OPTIONS  = -Wall
COMPILER = gcc

SRCDIR   = src
LIBDIR   = lib
BUILDDIR = build
TMPDIR   = tmp

.PHONY : all
all : $(BUILDDIR) $(TMPDIR) $(BUILDDIR)/ffprinter

$(BUILDDIR) :
	mkdir $(BUILDDIR)

$(TMPDIR) :
	mkdir $(TMPDIR)

.PHONY : run
run : $(BUILDDIR)/ffprinter
	./$(BUILDDIR)/ffprinter

$(BUILDDIR)/ffprinter : $(TMPDIR)/main.o                 $(TMPDIR)/ffprinter.o     \
						$(TMPDIR)/ffp_file.o             $(TMPDIR)/ffp_database.o  \
						$(TMPDIR)/ffp_fingerprint.o      $(TMPDIR)/ffp_directory.o \
						$(TMPDIR)/ffp_term.o             $(TMPDIR)/ffp_error.o     \
						$(TMPDIR)/ffp_scanmem.o          $(TMPDIR)/simple_bitmap.o
	$(COMPILER) $(OPTIONS) -static -o $(BUILDDIR)/ffprinter \
			$(TMPDIR)/main.o                 $(TMPDIR)/ffprinter.o     \
			$(TMPDIR)/ffp_file.o             $(TMPDIR)/ffp_database.o  \
			$(TMPDIR)/ffp_fingerprint.o      $(TMPDIR)/ffp_directory.o \
			$(TMPDIR)/ffp_term.o             $(TMPDIR)/ffp_error.o     \
			$(TMPDIR)/ffp_scanmem.o          $(TMPDIR)/simple_bitmap.o \
			-lssl -lcrypto -lreadline -lncurses

$(TMPDIR)/main.o : 			$(SRCDIR)/ffprinter.h \
							$(SRCDIR)/ffp_term.h  \
							$(SRCDIR)/main.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/main.c \
							-o $(TMPDIR)/main.o

$(TMPDIR)/ffprinter.o :   	$(SRCDIR)/ffprinter.h \
							$(SRCDIR)/ffprinter.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffprinter.c \
							-o $(TMPDIR)/ffprinter.o

$(TMPDIR)/ffp_file.o :    	$(SRCDIR)/ffprinter.h    \
							$(SRCDIR)/ffp_database.h \
							$(SRCDIR)/ffp_file.h     \
							$(SRCDIR)/ffp_file.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_file.c \
							-o $(TMPDIR)/ffp_file.o

$(TMPDIR)/ffp_database.o :  $(SRCDIR)/ffprinter.h    \
							$(SRCDIR)/ffp_database.h \
							$(SRCDIR)/ffp_database.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_database.c \
							-o $(TMPDIR)/ffp_database.o

$(TMPDIR)/ffp_fingerprint.o :   $(SRCDIR)/ffprinter.h       \
								$(SRCDIR)/ffp_fingerprint.h \
								$(SRCDIR)/ffp_fingerprint.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_fingerprint.c \
							-o $(TMPDIR)/ffp_fingerprint.o

$(TMPDIR)/ffp_directory.o : $(SRCDIR)/ffprinter.h     \
							$(SRCDIR)/ffp_directory.h \
							$(SRCDIR)/ffp_directory.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_directory.c \
							-o $(TMPDIR)/ffp_directory.o

$(TMPDIR)/ffp_term.o : 		$(SRCDIR)/ffprinter.h     \
							$(SRCDIR)/ffp_directory.h \
							$(SRCDIR)/ffp_term.h      \
							$(SRCDIR)/ffp_term.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_term.c \
							-o $(TMPDIR)/ffp_term.o

$(TMPDIR)/ffp_error.o : 		$(SRCDIR)/ffp_error.h \
							$(SRCDIR)/ffp_error.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_error.c \
							-o $(TMPDIR)/ffp_error.o

$(TMPDIR)/ffp_scanmem.o :  	$(SRCDIR)/ffp_scanmem.h \
							$(SRCDIR)/ffp_scanmem.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffp_scanmem.c \
							-o $(TMPDIR)/ffp_scanmem.o

$(TMPDIR)/simple_bitmap.o : $(LIBDIR)/simple_bitmap.h \
							$(LIBDIR)/simple_bitmap.c
	$(COMPILER) $(OPTIONS)  -c $(LIBDIR)/simple_bitmap.c \
							-o $(TMPDIR)/simple_bitmap.o

clean :
	rm \
		$(BUILDDIR)/ffprinter       \
		$(TMPDIR)/main.o            \
		$(TMPDIR)/ffprinter.o       \
		$(TMPDIR)/ffp_file.o        \
		$(TMPDIR)/ffp_database.o    \
		$(TMPDIR)/ffp_fingerprint.o \
		$(TMPDIR)/simple_bitmap.o   \
		$(TMPDIR)/ffp_directory.o   \
		$(TMPDIR)/ffp_term.o        \
		$(TMPDIR)/ffp_error.o       \
		$(TMPDIR)/ffp_scanmem.o
