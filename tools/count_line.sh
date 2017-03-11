#!/bin/bash

SRCDIR="../src"

wc -l \
    $SRCDIR/ffp_database.c  \
    $SRCDIR/ffp_database.h  \
    $SRCDIR/ffp_database_function_template.h    \
    $SRCDIR/ffp_directory.c \
    $SRCDIR/ffp_directory.h \
    $SRCDIR/ffp_error.c     \
    $SRCDIR/ffp_error.h     \
    $SRCDIR/ffp_file.c      \
    $SRCDIR/ffp_file.h      \
    $SRCDIR/ffp_fingerprint.c   \
    $SRCDIR/ffp_fingerprint.h   \
    $SRCDIR/ffp_term.c      \
    $SRCDIR/ffp_term.h      \
    $SRCDIR/ffprinter.c     \
    $SRCDIR/ffprinter.h     \
    $SRCDIR/ffprinter_function_template.h   \
    $SRCDIR/ffprinter_structure_template.h
