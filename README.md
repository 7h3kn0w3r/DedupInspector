# DedupInspector

A lightweight, portable, offline forensic utility for recovering files from Windows Data Deduplication volumes.

The tool reconstructs deduplicated files using:

* An exported NTFS `$MFT`
* A Deduplication **Stream** (`.ccc`) file
* A Deduplication **Chunk** (`.ccc`) file

No installation or Windows Data Deduplication service is required.

Built as a native Windows application using modern **C++17**, the Win32 API, and the C++ Standard Library with **CMake**, producing a single standalone executable (`DedupInspector.exe`).

**No Qt, .NET, Java, Electron, Node.js, or MFC.**

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

## 📖 Learn More

For a deep technical explanation of how Windows Data Deduplication works, including its internal structures, metadata, reparse points, ChunkStore, and reverse engineering details, read the accompanying research article:

➡️ **https://7h3kn0w3r.github.io/blog/windows-data-deduplication/#how-data-deduplication-work**

This article explains the concepts behind **DedupInspector** and the forensic techniques used by the tool.

