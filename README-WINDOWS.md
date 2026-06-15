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

## Tests

The suite under `test/` has two complementary layers, both driving Triangle
through its public `triangulate()` library API (the same entry point
`tricall.c` uses). Run everything with the PowerShell runner:

```powershell
pwsh -File test\run-tests.ps1
```

It compiles `triangle.c` (with `-DTRILIBRARY`) against the tests and exits
non-zero if anything fails.

### Layer 1 — Unity unit tests

`test/test_triangle.c` asserts human-readable invariants on small inputs:
triangle/vertex/edge counts, Euler's formula, and that area constraints force
refinement. It uses the [Unity](https://github.com/ThrowTheSwitch/Unity) C test
framework, vendored under `test/unity/`. (A `make test` target runs just this
layer from a Unix-style shell such as MSYS2.)

To add a unit test: write a `void test_xxx(void)` in `test/test_triangle.c`,
build a `struct triangulateio` input, call `triangulate(...)`, assert with
Unity's `TEST_ASSERT_*` macros, then register it with `RUN_TEST(test_xxx);` in
`main()`.

### Layer 2 — Golden corpus (characterization)

`test/golden_runner.c` runs a set of scenarios that exercise the **full feature
set in use** — PSLG input with segment markers, constrained triangulation,
quality meshing (`q`), regional area constraints (`a`) and region attributes
(`A`), holes, and neighbour/segment/edge output — and writes a deterministic
text dump of each output mesh. The baselines captured from a known-good build
live in `test/golden/*.txt`.

On each run the dumps are regenerated and compared **byte-for-byte** against the
baseline. Because Triangle is deterministic for a given input and switch set,
any difference — even one reordered triangle — fails the run. This is the
tripwire that makes it safe to refactor or remove unused code: cut, re-run, and
the corpus tells you immediately if the kept path changed.

When you intentionally change output (e.g. after a deliberate algorithm change,
**not** during dead-code removal), re-bless the baselines:

```powershell
pwsh -File test\run-tests.ps1 -Update
```

Review the resulting `git diff` of `test/golden/` to confirm the change is what
you expected before committing it.

To add a scenario: add a `scenario_xxx()` builder in `test/golden_runner.c`,
call it from `main()`, then run with `-Update` to create its baseline.

Note: Triangle aborts the whole process (via `exit()`) on a fatal input error
rather than returning a code, so tests should feed it valid geometry; this is a
limitation of testing a monolithic C program, not of the harness.
