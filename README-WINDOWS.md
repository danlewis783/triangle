# Building Triangle on 64-bit Windows

This note describes how to build Triangle on Windows 11 (64-bit Intel/AMD)
using only freely available software. The upstream `README` and `makefile`
target Unix; this file covers the Windows specifics.

## Reproducible Windows build (recommended)

Use PowerShell 7 on Windows 11 x64:

```powershell
.\build.ps1
.\verify-reproducible.ps1 -Offline
```

`build.ps1` bootstraps LLVM-MinGW 20260602 (Clang 22.1.7, UCRT) from
`native-dependencies.json`, verifies the archive SHA-256, and builds
`triangle.exe`, `triangle.o`, `tricall.exe`, and `triangle.dll` in `.build/out`.
`SHA256SUMS` records their hashes. No system compiler, make, or PATH setup is
required. Use `.build/out/triangle.dll` when copying the DLL to a consumer.

`bootstrap.ps1` can also run separately. Both scripts accept `-BuildRoot`
(default `.build`) and `-Offline`. Offline mode requires the pinned archive
in `<BuildRoot>/downloads/llvm-mingw-20260602.zip`; it still verifies the hash.
The hash is the same reviewed archive hash pinned by Triangle, not an
independent publisher signature. Extraction is cached; use a fresh build root
if the extracted compiler has been modified.

The build invokes the pinned compiler and LLD explicitly, suppresses PE linker
timestamps, fixes SOURCE_DATE_EPOCH, clears compiler include/library overrides,
and compiles stable source filenames with normalized paths. It restores the
calling process environment afterward. The exact source files and build scripts
are also build inputs: reproduce a particular revision with its original
manifest. Changing a compiler pin requires reviewing the archive URL and hash
and rerunning verification.

`verify-reproducible.ps1` extracts tools and builds in two new directories,
including a path containing spaces. It compares all four artifacts byte for
byte through SHA-256, runs the standalone and callable-library examples, and
checks the DLL exports. Verification directories are retained under the build
root for inspection. This demonstrates reproducibility on the tested Windows
host; a second-machine comparison is a separate check.

The manual workflows below remain available, but do not enforce compiler pins
or deterministic settings.

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

To build Triangle as a shared library (`triangle.dll`) for dynamic loaders
such as JNA:

```powershell
mingw32-make shared
```

### Option B — invoke Clang directly

```powershell
clang -O2 -DCPU86 -DNO_TIMER -o triangle.exe triangle.c -lm
```

To also build the callable library object and the sample driver:

```powershell
clang -O2 -DTRILIBRARY -DCPU86 -DNO_TIMER -c -o triangle.o triangle.c
clang -O2 -DCPU86 -DNO_TIMER -o tricall.exe tricall.c triangle.o -lm
```

Or the shared library:

```powershell
clang -O2 -DTRILIBRARY -DCPU86 -DNO_TIMER -shared "-Wl,--export-all-symbols" -o triangle.dll triangle.c
```

The compile prints some deprecation and `%lx`-format warnings from the
2005-era C code; these are harmless and do not affect correctness.

## The DLL for JNA consumers

Upstream Triangle has no notion of a DLL: the author's "library" form is
`triangle.o` compiled with `-DTRILIBRARY` (see `make trilibrary`), and
`tricall.c` is merely an example *client* program. The `triangle.dll` name
used here follows the source/JNA convention (`Native.load("triangle", ...)`);
the legacy `tricall.dll` name was an artifact of the old SWIG/JNI wrapper,
not anything the original author intended.

Build notes:

- `-DTRILIBRARY` is required — without it `triangle.c` compiles the
  standalone `main()` and there is no `triangulate()` to call.
- `-Wl,--export-all-symbols` is required — the source has no
  `__declspec(dllexport)` annotations, and JNA resolves `triangulate` and
  `trifree` through the PE export table.
- The consuming project loads the DLL from its classpath at
  `src/main/resources/win32-x86-64/triangle.dll` (e.g.
  `C:\dev\triangle-java`). Copy the freshly built DLL there after a rebuild.

Verify the exports if in doubt:

```powershell
llvm-objdump -p triangle.dll | Select-String "triangulate|trifree"
```

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
