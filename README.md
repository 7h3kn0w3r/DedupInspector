# Dedup Explorer

A lightweight, portable, offline forensic utility for recovering files from
Windows Data Deduplication volumes, given:

- An exported `$MFT`
- The volume's `System Volume Information\Dedup` folder (or its
  `ChunkStore` subfolder directly)

No Qt, .NET, Java, Electron, Node, or MFC — pure C++17 + Win32 + STL,
built with CMake into a single native `DedupExplorer.exe`.

## Build

Requires Windows + either MSVC (Visual Studio 2019/2022 Build Tools) or
MinGW-w64. This project cannot be cross-compiled from Linux/macOS as-is
(Win32 API + Windows Compression API).

### MSVC (Developer Command Prompt / PowerShell with VS tools on PATH)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The binary lands at `build\bin\Release\DedupExplorer.exe` (statically
linked CRT via the `/MT` switch in `CMakeLists.txt`, so it runs on a bare
Windows install without a VC++ Redistributable).

### MinGW-w64

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Produces `build/bin/DedupExplorer.exe`, statically linked
(`-static -static-libgcc -static-libstdc++`) for the same
copy-and-run portability.

## Project layout

```
DedupExplorer/
  CMakeLists.txt
  include/
    dedup_types.h        shared structs (DedupFileEntry, ChunkHashRecord, ...)
  src/
    main.cpp              WinMain, COM/CommonControls init, message loop
    mainwindow.h/.cpp      single-window Win32 UI (MFT Explorer-style)
    mft_parser.h/.cpp      $MFT record/attribute parsing (FILE_NAME, REPARSE_POINT)
    reparse_parser.h/.cpp  IO_REPARSE_TAG_DEDUP payload parsing
    stream_parser.h/.cpp   locates Stream_* files, extracts per-file CKHR blob
    ckhr_parser.h/.cpp     Chunk Hash Record extraction
    chunk_parser.h/.cpp    .ccc container indexing, chunk read + decompression
    recovery_engine.h/.cpp reassembles original files from resolved chunks
    utils.h/.cpp           file I/O, GUID/hex formatting, path helpers
  resources/
    app.rc, app.manifest   version info + Common Controls v6 manifest
```

## ⚠️ On format accuracy (read before trusting recovered output)

Windows Data Deduplication's on-disk structures — the `IO_REPARSE_TAG_DEDUP`
reparse point payload, the `Stream_*` container internals, and `CKHR`
("Chunk Hash Record") layout — are **not publicly documented by
Microsoft**. Everything this tool assumes about those formats comes from
community DFIR reverse-engineering and may drift across Windows Server
versions/dedup job format revisions.

To keep this tractable, every version-sensitive assumption is isolated in
one place so you can correct it quickly against your own samples:

- `reparse_parser.h` — `kOffset*` constants for original size / stream
  header id / chunk store GUID inside the reparse payload.
- `stream_parser.cpp` — `StreamEntryHeader` layout for locating a file's
  CKHR blob inside a `Stream_*` file.
- `ckhr_parser.h` — `CKHR` fixed-field layout (container index, offset,
  lengths, compression flag, SHA-256 hash).
- `chunk_parser.cpp` — container file extension matched (`*.ccc`) and the
  `COMPRESS_ALGORITHM_XPRESS_HUFF` id passed to the Windows Compression
  API (`compressapi.h`) for chunk decompression; swap in
  `COMPRESS_ALGORITHM_XPRESS` or `COMPRESS_ALGORITHM_MSZIP` if a hex dump
  of your samples shows a different scheme.

**Recommended validation workflow:** take one small, known test file on a
lab volume with dedup enabled, run a dedup optimization job on it, then
hex-dump its reparse point (`fsutil reparsepoint query`, or read the
`$REPARSE_POINT` attribute directly from a raw MFT export) and the
relevant `Stream_*` / `.ccc` files to confirm/adjust the offsets above
before relying on recovered output for casework.

## Recovery behavior

- Chunks that can't be located/read are zero-filled in the output so file
  length and the offsets of successfully recovered chunks stay correct —
  this keeps partial recoveries forensically useful rather than corrupt.
- `Recover All` groups files by chunk store GUID and re-indexes chunk
  containers per group, so a scan spanning multiple dedup jobs/volumes
  still resolves each file against the right container set.
- Output paths mirror the original directory structure (as reconstructed
  from MFT parent-record chains) under your chosen output folder.

## UI

Single fixed window, no tabs/ribbon/docking, matching the brief:

```
[MFT File]     [Browse]
[Dedup Folder] [Browse]
[        Scan        ]
+--------------------------------------------------+
| Filename | Original Size | Chunks | Recovery      |
+--------------------------------------------------+
Selected File: ...
Original Size: ...   Chunk Count: ...
Chunk Store GUID: ...   Stream Header: ...
Recovery Status: ...
[Recover Selected]  [Recover All]
[==== progress ====]
```
