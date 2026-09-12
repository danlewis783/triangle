# Building Triangle with Microsoft MSVC on Windows x64

Use PowerShell 7 on Windows 11 x64:

```powershell
.\build.ps1
.\verify-reproducible.ps1 -Offline
```

The build uses Microsoft `cl.exe`, `link.exe`, `lib.exe`, and `nmake.exe`,
with Microsoft C runtime libraries and Windows SDK headers/libraries.
Verification uses Microsoft `dumpbin.exe`. LLVM, MinGW, GCC, and GNU make
are no longer used.

## Prerequisites

Install Visual Studio or Visual Studio Build Tools with **Desktop development
with C++**, including the versions pinned in `native-dependencies.json`:

- MSVC tools: **14.51.36231** (Hostx64/x64).
- Windows SDK: **10.0.26100.0**.

`bootstrap.ps1` locates Visual Studio using the installed `vswhere.exe` and
the Windows SDK using the registry, then validates the required paths. It
does not install software or download archives. It fails if the pinned
versions are unavailable; install them through Visual Studio Installer, or
deliberately update the pins and rerun verification.

No developer prompt or global PATH changes are needed. All three scripts
retain `-BuildRoot` (default `.build`) and `-Offline` for compatibility.
Every build is now offline; there is no archive cache requirement.

## Outputs

The supported outputs are in `.build/out`:

| File | Purpose |
| --- | --- |
| `triangle.exe` | Standalone mesher |
| `triangle.obj` | Callable MSVC COFF object, replacing `triangle.o` |
| `triangle-static.lib` | Static library |
| `tricall.exe` | Sample linked to the static library |
| `triangle.dll` | Shared library |
| `triangle.lib` | DLL import library, distinct from the static library |
| `msvc-dll.exe` | DLL triangulation and allocation smoke test |

`SHA256SUMS` records all seven hashes; a copy of the dependency manifest is
saved alongside them. The build always recompiles. Old root-level binaries
or LLVM files left in `.build` are not inputs; use `.build/out/triangle.dll`
for consumers.

## Build settings

The NMAKE makefile compiles C with `/O2 /fp:strict /MT /DNO_TIMER`.
Strict floating-point behavior preserves the arithmetic ordering needed by
Triangle's robust predicates and disables contraction. The x64 target uses
SSE2; it does not define `CPU86` or attempt unsupported x87 precision control.

The Microsoft runtime is linked statically (`/MT`), so deployment does not
require a separate Visual C++ runtime DLL. Windows system DLLs are still
required. `TRILIBRARY` enables the callable API, and `triangle.def` explicitly
exports `triangulate` and `trifree`.

The Win64 pointer tagging fixes using `uintptr_t` remain in place. Verbose
pointer diagnostics now use `%p` with `void *`, avoiding 64-bit truncation.

References: [Microsoft floating-point settings](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior)
and [x64 floating-point control limitations](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/control87-controlfp-control87-2).

## Reproducibility and verification

The build selects pinned tool/header/library directories, clears compiler,
linker, and make environment overrides, and restores the process environment
even on failure. It compiles stable relative source names in the output
directory. `/Brepro`, `/experimental:deterministic`, and `/pathmap` suppress
variable timestamps and normalize embedded build paths; incremental linking
is disabled. Some of these reproducibility switches are experimental or
undocumented, so changing the toolchain requires rerunning verification.

Verification builds in two unique directories (one containing spaces),
compares all seven artifacts by SHA-256, runs `A.poly`, runs quality meshing
with `-pq30a5C`, exercises the static library sample and DLL smoke test, and
checks both exports with `dumpbin`. Results are retained for inspection.

This checks reproducibility on the current host. Version pins are not hashes
of every installed Microsoft file; identical sources, scripts, and toolchain
inputs are required for a cross-machine comparison.

## DLL consumers, including JNA

Load `triangle.dll` (for example, JNA `Native.load("triangle", ...)`).
The target remains Windows x64 with double-precision coordinates and the
existing `triangulate`/`trifree` interface.

Free memory allocated by Triangle through **that DLL's `trifree`**. Free
caller-owned inputs using their original allocator. This matters with the
statically linked runtime: do not free DLL-owned outputs with a caller's
`free`, Java allocator, or another CRT.

Copy `.build/out/triangle.dll` into a consumer's
`src/main/resources/win32-x86-64/triangle.dll` when updating that consumer.
Native clients can link `triangle.lib` for the DLL or `triangle-static.lib`
for static linking; the static library requires compatible MSVC `/MT` settings.

## Direct NMAKE use

From an **x64 Native Tools** prompt at the project root:

```text
nmake /nologo
nmake /nologo trilibrary
nmake /nologo shared
nmake /nologo distclean
```

Direct NMAKE writes to the current directory and uses the prompt's tools;
it does not enforce manifest pins or isolate environment overrides.
Prefer `build.ps1` for the verified build. The makefile now uses Microsoft
NMAKE syntax; the original Unix/GNU make workflow has been replaced.
