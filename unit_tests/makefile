OPTIONS = -Wall
COMPILER = gcc

SRCDIR  = ../src
LIBDIR  = ../lib
TMPDIR  = tmp
BUILDDIR = build

.PHONY : all
all : $(BUILDDIR) $(TMPDIR) $(BUILDDIR)/test_template

$(BUILDDIR) :
	mkdir $(BUILDDIR)

$(TMPDIR) :
	mkdir $(TMPDIR)

.PHONY : run
run : $(BUILDDIR)/test_template
	./$(BUILDDIR)/test_template

$(BUILDDIR)/test_template : $(TMPDIR)/test_template.o $(TMPDIR)/simple_bitmap.o $(TMPDIR)/ffprinter.o
	$(COMPILER) $(OPTIONS) -o $(BUILDDIR)/test_template $(TMPDIR)/test_template.o $(TMPDIR)/ffprinter.o $(TMPDIR)/simple_bitmap.o -lssl -lcrypto

$(TMPDIR)/test_template.o : test_template.c
	$(COMPILER) $(OPTIONS)  -c test_template.c \
							-o $(TMPDIR)/test_template.o

$(TMPDIR)/ffprinter.o : $(SRCDIR)/ffprinter.h \
						$(SRCDIR)/ffprinter.c
	$(COMPILER) $(OPTIONS)  -c $(SRCDIR)/ffprinter.c \
							-o $(TMPDIR)/ffprinter.o

$(TMPDIR)/simple_bitmap.o : $(LIBDIR)/simple_bitmap.h $(LIBDIR)/simple_bitmap.c 
	$(COMPILER) $(OPTIONS)  -c $(LIBDIR)/simple_bitmap.c \
							-o $(TMPDIR)/simple_bitmap.o

clean :
	rm $(BUILDDIR)/test_template $(TMPDIR)/ffprinter.o $(TMPDIR)/simple_bitmap.o $(TMPDIR)/test_template.o

