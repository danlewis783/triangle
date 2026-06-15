# Building Triangle on 64-bit Windows

This note describes how to build Triangle on Windows 11 (64-bit Intel/AMD)
using only freely available software. The upstream `README` and `makefile`
target Unix; this file covers the Windows specifics.

## Toolchain: LLVM-MinGW (Clang)

We use **LLVM-MinGW** — a self-contained distribution bundling Clang, the
LLD linker, and a complete MinGW runtime and headers. It needs no Microsoft
Visual Studio installation and is fully portable (just unzip it).

1. Download the latest `llvm-mingw-*-ucrt-x86_64.zip` from
   <https://github.com/mstorsjo/llvm-mingw/releases>.
2. Unzip it and place the contents at `C:\tools\llvm-mingw` (so that
   `C:\tools\llvm-mingw\bin\clang.exe` exists).
3. Add `C:\tools\llvm-mingw\bin` to your `PATH`.

Verify the compiler is reachable:

```powershell
clang --version
```

It should report a `Target: x86_64-w64-windows-gnu` build.

## Required compiler flags

Two preprocessor switches are needed on Windows (already baked into the
`makefile`'s `CSWITCHES`):

| Flag         | Why it is needed                                                  |
|--------------|-------------------------------------------------------------------|
| `-DCPU86`    | Sets the x86 FPU control word so the exact arithmetic is robust.  |
| `-DNO_TIMER` | Drops the Unix-only `<sys/time.h>` timing code, absent on Windows.|

> **Note on the source.** Triangle stores flag bits in the low bits of its
> pointers and originally cast them through `unsigned long`. On Windows
> (LLP64) that type is only 32 bits, which truncates 64-bit pointers and
> crashes the program. The source has been updated to use `uintptr_t`
> (from `<stdint.h>`) for those casts, so it now runs correctly on Win64.
> This change is harmless on Unix builds.

## Build

From the project directory (`C:\dev\triangle`) in a PowerShell prompt:

### Option A — the makefile

```powershell
mingw32-make
```

This produces `triangle.exe`. (LLVM-MinGW ships `mingw32-make.exe`, and its
Clang front end is also available as `cc`, which the makefile invokes.)

### Option B — invoke Clang directly

```powershell
clang -O2 -DCPU86 -DNO_TIMER -o triangle.exe triangle.c -lm
```

To also build the callable library object and the sample driver:

```powershell
clang -O2 -DTRILIBRARY -DCPU86 -DNO_TIMER -c -o triangle.o triangle.c
clang -O2 -DCPU86 -DNO_TIMER -o tricall.exe tricall.c triangle.o -lm
```

The compile prints some deprecation and `%lx`-format warnings from the
2005-era C code; these are harmless and do not affect correctness.

## Verify the build

Run the bundled sample input:

```powershell
.\triangle.exe A.poly
```

This should exit cleanly and write `A.1.node`, `A.1.ele`, and `A.1.poly`.
A quick test of the quality-meshing / refinement path:

```powershell
.\triangle.exe -pq30a5 A.poly
```

It should add Steiner points and report a larger mesh (≈76 vertices) with
exit code 0.
