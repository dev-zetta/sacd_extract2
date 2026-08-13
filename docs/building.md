# Building sacd_extract2

The repository uses one host-aware entry point for Linux, macOS, and Windows:

```bash
scripts/build.sh
```

The script detects the host platform, checks required tools, configures CMake,
builds the command-line extractor, runs CTest, prepares the extractor as a
Tauri sidecar, installs locked GUI dependencies, and runs the Rust GUI tests
and JavaScript syntax checks.

## Linux

Install C and C++ compilers, Git, and CMake 3.16 or newer. Building the Tauri GUI also
requires Node.js 22 or newer, npm, the stable Rust toolchain, WebKitGTK 4.1,
librsvg, and `patchelf`. XML export uses the bundled writer and does not require
libxml2. CMake downloads the pinned libFLAC 1.5.0 source on the first clean
configuration and statically links it into the extractor; no system libFLAC
development or runtime package is required. For an offline build, set CMake's
`FETCHCONTENT_SOURCE_DIR_FLAC` to an unpacked FLAC 1.5.0 source directory.

On Debian or Ubuntu, install the native toolchain with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake git pkg-config file libssl-dev \
  libwebkit2gtk-4.1-dev libxdo-dev librsvg2-dev patchelf
```

Install Node.js 22 or newer using the method appropriate for your distribution
and install Rust through [rustup](https://rustup.rs/), then build from the
repository root:

```bash
scripts/build.sh
```

The command-line executable is written to `build/linux/sacd_extract`.

## Windows

The supported Windows build uses 64-bit MinGW-w64 in an MSYS2 MINGW64 shell.
The removed Visual Studio project is not used. Install the required MSYS2
packages with:

```bash
pacman -Syu
pacman -S --needed diffutils git mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-libiconv
```

Restart the MINGW64 shell if the MSYS2 update requests it. Install Node.js 22
or newer plus the stable MSVC Rust toolchain, ensure `node`, `npm`, `cargo`, and
`rustc` are visible in that shell, then run:

```bash
scripts/build.sh
```

The script selects Ninja and enables `SACD_WINDOWS_STATIC` automatically. The
resulting `build/windows/sacd_extract.exe` statically includes its non-system
MinGW, pthread, iconv, and libFLAC dependencies.

## macOS (Apple Silicon)

The supported macOS target is native Apple Silicon (`arm64`) with macOS 11 as
its deployment baseline. Install the Xcode Command Line Tools, CMake, Node.js
22 or newer, and the stable Rust toolchain, then run on an Apple Silicon Mac:

```bash
xcode-select --install
brew install cmake node@22
scripts/build.sh --platform macos
```

The command-line executable is written to `build/macos/sacd_extract`. CMake
uses Apple's system iconv and statically bundles libFLAC, so the release does
not depend on Homebrew libraries. Packaging uses Tauri's
`aarch64-apple-darwin` sidecar name and creates an ad-hoc-signed DMG. The
community release is not Apple-notarized, so macOS may require approval under
System Settings > Privacy & Security on first launch.

## Build options

| Option | Purpose |
| --- | --- |
| `--platform auto\|linux\|macos\|windows` | Detect the host or require a specific host platform |
| `--build-dir DIR` | Override `build/linux`, `build/macos`, or `build/windows` |
| `--build-type TYPE` | Select the CMake build type; default `Release` |
| `--package` | Create CLI archives, a host GUI bundle, and checksums |
| `--skip-gui` | Build and test only the command-line extractor |
| `--sanitizers` | Enable ASan and UBSan for a Linux build |

Run `scripts/build.sh --help` for the current option list.

## Release packages

Create host-platform archives with:

```bash
scripts/build.sh --package
```

Artifacts are written under `build/dist`. The build script verifies that the
Tauri sidecar is byte-identical to the extractor tested earlier in the same
invocation. Packaging produces a single-file Linux AppImage, Apple Silicon
macOS DMG, or Windows NSIS installer. It rejects libxml and libFLAC runtime
links on Linux, rejects Homebrew and dynamic libFLAC links on macOS, rejects
non-system MinGW runtime DLLs on Windows, and writes SHA-256 checksum files.

[Tagged releases](https://github.com/dev-zetta/sacd_extract2/releases) provide
ready-to-run Linux and Windows x86-64 packages plus Apple Silicon macOS CLI and
DMG packages.
