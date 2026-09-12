# Microsoft NMAKE build. Prefer .\build.ps1 for pinned tools and isolated outputs.
# Direct use requires an x64 Native Tools command prompt; outputs go here.
CC = cl.exe
LD = link.exe
AR = lib.exe
CFLAGS = /nologo /TC /O2 /fp:strict /MT /DNO_TIMER /D_CRT_SECURE_NO_WARNINGS /Brepro /experimental:deterministic /pathmap:"$(MAKEDIR)=/triangle"
LDFLAGS = /nologo /MACHINE:X64 /INCREMENTAL:NO /Brepro

all: triangle.exe trilibrary shared msvc-dll.exe

trilibrary: triangle-static.lib tricall.exe

shared: triangle.dll

triangle-main.obj: triangle.c
	$(CC) $(CFLAGS) /c /Fotriangle-main.obj triangle.c

triangle.obj: triangle.c triangle.h
	$(CC) $(CFLAGS) /DTRILIBRARY /c /Fotriangle.obj triangle.c

tricall.obj: tricall.c triangle.h
	$(CC) $(CFLAGS) /c /Fotricall.obj tricall.c

triangle.exe: triangle-main.obj
	$(LD) $(LDFLAGS) /OUT:triangle.exe triangle-main.obj

triangle-static.lib: triangle.obj
	$(AR) /nologo /MACHINE:X64 /Brepro /OUT:triangle-static.lib triangle.obj

tricall.exe: tricall.obj triangle-static.lib
	$(LD) $(LDFLAGS) /OUT:tricall.exe tricall.obj triangle-static.lib

triangle.dll: triangle.obj triangle.def
	$(LD) $(LDFLAGS) /DLL /DEF:triangle.def /IMPLIB:triangle.lib /OUT:triangle.dll triangle.obj

distclean:
	-del /Q triangle.exe tricall.exe triangle-main.obj triangle.obj tricall.obj triangle-static.lib triangle.dll triangle.lib triangle.exp msvc-dll.obj msvc-dll.exe

msvc-dll.obj: test\msvc-dll.c triangle.h
	$(CC) $(CFLAGS) /c /I. /Fomsvc-dll.obj test\msvc-dll.c

msvc-dll.exe: msvc-dll.obj triangle.dll
	$(LD) $(LDFLAGS) /OUT:msvc-dll.exe msvc-dll.obj triangle.lib
