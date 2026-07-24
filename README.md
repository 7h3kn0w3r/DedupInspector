# DedupExplorer

**DedupExplorer** is an open-source Windows forensic tool for exploring, analyzing, and recovering NTFS Data Deduplication files from raw metadata and ChunkStore data.

## Download

Precompiled Windows binaries are available on the **Releases** page.

➡️ Download the latest version from **Releases**.

## Features

* Native Win32 GUI
* NTFS `$MFT` parser
* Data Deduplication metadata parser
* ChunkStore parser
* Offline file recovery
* Built with modern C++ and CMake

## Build from Source

```bash
git clone https://github.com/<your-username>/DedupExplorer.git
cd DedupExplorer

cmake -B build
cmake --build build --config Release
```

## License

MIT License.
