# makefile for Triangle
#
# Type "make" to compile Triangle as a library object.
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
# An example CSWITCHES line is:
#
#   CSWITCHES = -O -DNO_TIMER -DLINUX

# For a 64-bit Windows build with the LLVM-MinGW (clang) toolchain:
#   -DCPU86    enables the correct x86 FPU control word for robust arithmetic
#   -DNO_TIMER drops the Unix-only <sys/time.h> timing code
CSWITCHES = -O -DCPU86 -DNO_TIMER

# RM should be set to the name of your favorite rm (file deletion program).

RM = /bin/rm

# The action starts here.

# `test' shares its name with the test/ directory, so mark targets phony.
.PHONY: all trilibrary test distclean

all: $(BIN)triangle.o

trilibrary: all

$(BIN)triangle.o: $(SRC)triangle.c $(SRC)triangle.h
	$(CC) $(CSWITCHES) -c -o $(BIN)triangle.o \
		$(SRC)triangle.c

# Build and run the Unity test suite (see test/test_triangle.c).  This recipe
# assumes a Unix-style shell; on a plain Windows/PowerShell prompt run
# "pwsh -File test/run-tests.ps1" instead.
test: $(SRC)triangle.c $(SRC)triangle.h test/test_triangle.c test/unity/unity.c
	$(CC) $(CSWITCHES) -I. -Itest/unity \
		-o test/test_triangle $(SRC)triangle.c \
		test/unity/unity.c test/test_triangle.c -lm
	./test/test_triangle

distclean:
	$(RM) $(BIN)triangle.o \
		test/test_triangle test/test_triangle.exe
