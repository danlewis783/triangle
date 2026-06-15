# makefile for Triangle
#
# Type "make" to compile Triangle.
#
# After compiling, type "triangle -h" to read instructions for using.
#
# Type "make trilibrary" to compile Triangle as an object file (triangle.o).
#
# Type "make distclean" to delete all object and executable files.

# SRC is the directory in which the C source files are, and BIN is the
#   directory where you want to put the executable programs.  By default,
#   both are the current directory.

SRC = ./
BIN = ./

# CC should be set to the name of your favorite C compiler.

CC = cc

# CSWITCHES is a list of all switches passed to the C compiler.  I strongly
#   recommend using the best level of optimization.  I also strongly
#   recommend timing each level of optimization to see which is the
#   best.  For instance, when I had a DEC Alpha using DEC's optimizing
#   compiler, the -O2 switch generated a notably faster version of Triangle
#   than the -O3 switch.  Go figure.
#
# By default, Triangle uses double precision floating point
#   numbers.  If you prefer single precision, use the -DSINGLE switch.
#   Double precision uses more memory, but improves the resolution of
#   the meshes you can generate with Triangle.  It also reduces the
#   likelihood of a floating exception due to overflow.  Also, it is
#   much faster than single precision on many architectures.  I recommend
#   double precision unless you want to generate a mesh for which you do
#   not have enough memory to use double precision.
#
# If yours is not a Unix system, use the -DNO_TIMER switch to eliminate the
#   Unix-specific timer code.
#
# To get the exact arithmetic to work right on an Intel processor, use the
#   -DCPU86 switch with Microsoft C, or the -DLINUX switch with gcc running
#   on Linux.  The floating-point arithmetic might not be robust otherwise.
#   Please see http://www.cs.cmu.edu/~quake/robust.pc.html for details.
#
# If you are modifying Triangle, I recommend using the -DSELF_CHECK switch
#   while you are debugging.  Defining the SELF_CHECK symbol causes
#   Triangle to include self-checking code.  Triangle will execute more
#   slowly, however, so be sure to remove this switch before compiling a
#   production version.
#
# If the size of the Triangle binary is important to you, you may wish to
#   generate a reduced version of Triangle.  The -DREDUCED switch gets rid
#   of all features that are primarily of research interest.  Specifically,
#   defining the REDUCED symbol eliminates the -i, -F, -s, and -C switches.
#   The -DCDT_ONLY switch gets rid of all meshing algorithms above and beyond
#   constrained Delaunay triangulation.  Specifically, defining the CDT_ONLY
#   symbol eliminates the -r, -q, -a, -u, -D, -S, and -s switches.  The
#   REDUCED and CDT_ONLY symbols may be particularly attractive when Triangle
#   is called by another program that does not need all of Triangle's
#   features; in this case, these switches should appear as part of
#   "TRILIBDEFS" below.
#
#
# An example CSWITCHES line is:
#
#   CSWITCHES = -O -DNO_TIMER -DLINUX

# For a 64-bit Windows build with the LLVM-MinGW (clang) toolchain:
#   -DCPU86    enables the correct x86 FPU control word for robust arithmetic
#   -DNO_TIMER drops the Unix-only <sys/time.h> timing code
CSWITCHES = -O -DCPU86 -DNO_TIMER

# TRILIBDEFS is a list of definitions used to compile an object code version
#   of Triangle (triangle.o) to be called by another program.  The file
#   "triangle.h" contains detailed information on how to call triangle.o.
#
# The -DTRILIBRARY should always be used when compiling Triangle into an
#   object file.
#
# An example TRILIBDEFS line is:
#
#   TRILIBDEFS = -DTRILIBRARY -DREDUCED -DCDT_ONLY

TRILIBDEFS = -DTRILIBRARY

# RM should be set to the name of your favorite rm (file deletion program).

RM = /bin/rm

# The action starts here.

# `test' shares its name with the test/ directory, so mark targets phony.
.PHONY: all trilibrary test distclean

all: $(BIN)triangle

trilibrary: $(BIN)triangle.o $(BIN)tricall

$(BIN)triangle: $(SRC)triangle.c
	$(CC) $(CSWITCHES) -o $(BIN)triangle $(SRC)triangle.c -lm

$(BIN)tricall: $(BIN)tricall.c $(BIN)triangle.o
	$(CC) $(CSWITCHES) -o $(BIN)tricall $(SRC)tricall.c \
		$(BIN)triangle.o -lm

$(BIN)triangle.o: $(SRC)triangle.c $(SRC)triangle.h
	$(CC) $(CSWITCHES) $(TRILIBDEFS) -c -o $(BIN)triangle.o \
		$(SRC)triangle.c

# Build and run the Unity test suite (see test/test_triangle.c).  This recipe
# assumes a Unix-style shell; on a plain Windows/PowerShell prompt run
# "pwsh -File test/run-tests.ps1" instead.
test: $(SRC)triangle.c $(SRC)triangle.h test/test_triangle.c test/unity/unity.c
	$(CC) $(CSWITCHES) -DTRILIBRARY -I. -Itest/unity \
		-o test/test_triangle $(SRC)triangle.c \
		test/unity/unity.c test/test_triangle.c -lm
	./test/test_triangle

distclean:
	$(RM) $(BIN)triangle $(BIN)triangle.o $(BIN)tricall test/test_triangle
